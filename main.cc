#include "galaxy.hh"
#include "protoset.hh"
#include "filter.hh"
#include "settings.hh"
#include "util/instrumentation.hh"

#include <fmt/ostream.h>
#include <fmt/format.h>
#include <getopt.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <fstream>
#include <iostream>
#include <map>

static std::mutex mutex1, mutex2;
static std::map<int, std::vector<std::pair<int, int>>> seedsToCheckMap;
static std::vector<std::pair<int, int>> *seedsToCheck = nullptr;
static size_t currIndex = 0, totalSize = 0;
static int current = -1, currMax = -1, starCount = 64;
struct SeedBlock {
    int begin;
    int end;
};
static std::vector<SeedBlock> seedBlocks;
static std::atomic_size_t nextSeedBlock;
static bool useBlockScheduling = false;

static bool poseOnly = false;
static std::atomic_int found;
static std::chrono::time_point<std::chrono::steady_clock> *startTime;

class OutputWriter {
public:
    explicit OutputWriter(const std::string &filename) : stream(filename) {
        fmt::print(stream, "Seed,Star Count\n");
        thread = std::thread([this] { run(); });
    }

    ~OutputWriter() {
        finish();
    }

    void enqueue(std::string row) {
        enqueue(std::move(row), {});
    }

    void enqueue(std::string row, std::vector<OwnedOutputRecord> records) {
        std::unique_lock lock(mutex);
        notFull.wait(lock, [this] { return queue.size() < 1024 || stopping; });
        if (stopping) return;
        queue.push_back({std::move(row), std::move(records)});
        notEmpty.notify_one();
    }

    void finish() {
        {
            std::lock_guard lock(mutex);
            if (stopping) return;
            stopping = true;
        }
        notEmpty.notify_all();
        notFull.notify_all();
        if (thread.joinable()) thread.join();
        stream.close();
    }

private:
    void run() {
        while (true) {
            WorkItem item;
            {
                std::unique_lock lock(mutex);
                notEmpty.wait(lock, [this] { return !queue.empty() || stopping; });
                if (queue.empty()) break;
                item = std::move(queue.front());
                queue.pop_front();
                notFull.notify_one();
            }
            stream << item.row;
            for (const auto &record: item.records) {
                auto found = channelStreams.find(record.channel);
                if (found == channelStreams.end()) {
                    found = channelStreams.emplace(
                        record.channel,
                        std::ofstream(fmt::format("plugin-output-{}.bin", record.channel),
                                      std::ios::binary)).first;
                }
                found->second.write(record.data.data(),
                                    static_cast<std::streamsize>(record.data.size()));
            }
        }
    }

    struct WorkItem {
        std::string row;
        std::vector<OwnedOutputRecord> records;
    };

    std::ofstream stream;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable notEmpty;
    std::condition_variable notFull;
    std::deque<WorkItem> queue;
    std::map<std::uint32_t, std::ofstream> channelStreams;
    bool stopping = false;
};

static OutputWriter *outputWriter;

static bool getNextSeed(int &seed, int &blockCurrent, int &blockEnd) {
    if (useBlockScheduling) {
        if (blockCurrent >= blockEnd) {
            auto index = nextSeedBlock.fetch_add(1, std::memory_order_relaxed);
            if (index >= seedBlocks.size()) return false;
            blockCurrent = seedBlocks[index].begin;
            blockEnd = seedBlocks[index].end;
        }
        seed = blockCurrent++;
        return true;
    }

    std::unique_lock lock(mutex1);
    if (current >= currMax) {
        if (++currIndex >= totalSize) return false;
        current = (*seedsToCheck)[currIndex].first;
        currMax = (*seedsToCheck)[currIndex].second;
    }
    seed = current++;
    return true;
}

/*
void outputFunc(const Star *star) {
    auto seed = star->galaxy->seed;
    auto sc = star->galaxy->starCount;
    if (hasStars) {
        fmt::print(output[1],
                   "{},{},{},{},{:.3f},{:.3f},{}\n",
                   seed,
                   sc,
                   star->id,
                   star->typeName(),
                   star->position.magnitude(),
                   pow(star->luminosity, 0.33000001311302185),
                   star->name);
    }
    if (hasPlanets) {
        for (auto &planet: star->planets) {
            fmt::print(output[0],
                       "{},{},{},{},{},{}",
                       seed,
                       sc,
                       planet->id,
                       planet->orbitAround,
                       planet->theme,
                       planet->singularity & EPlanetSingularity::TidalLocked ? 1 : 0);
            for (int i = 1; i < 15; ++i) {
                fmt::print(output[0], ",{}", planet->veinSpot[i]);
            }
            fmt::print(output[0], "\n");
        }
    }
};
*/

static void calc(std::uint32_t workerIndex) {
    dspugen::util::instrumentation::resetThread();
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();
    initPluginThread(workerIndex);
    auto serializeLegacyFilters = pluginsRequireSerializedFilters();
    int blockCurrent = 0;
    int blockEnd = 0;
    while (true) {
        int seed;
        if (!getNextSeed(seed, blockCurrent, blockEnd)) break;
        if (seed % 500000 == 0) {
            fmt::print(std::cerr, "Processed to: {},{}. Currently found: {}. {}ms elapsed.\n", seed, starCount, found.load(std::memory_order_relaxed), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *startTime).count());
        }
        auto galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount);
        if (!galaxy) continue;
        FilterInvocation invocation;
        std::unique_lock<std::mutex> legacyFilterLock(mutex2, std::defer_lock);
        if (serializeLegacyFilters) legacyFilterLock.lock();
        auto requirements = prePlanetRequirements();
        if (requirements & (NeedsPlanetTopology | NeedsPlanetTheme | NeedsVeinMetadata | NeedsGas))
            galaxy->ensureAllPlanets();
        if (requirements & NeedsGas) galaxy->ensureAllGas();
        if (!runPrePlanetFilters(galaxy, invocation)) {
            galaxy->release();
            continue;
        }
        requirements = filterRequirements();
        if (requirements & (NeedsPlanetTopology | NeedsPlanetTheme | NeedsVeinMetadata | NeedsGas))
            galaxy->ensureAllPlanets();
        if (requirements & NeedsGas) galaxy->ensureAllGas();
        if (!runFilters(galaxy, invocation)) {
            galaxy->release();
            continue;
        }
        requirements = outputRequirements();
        if (requirements & (NeedsPlanetTopology | NeedsPlanetTheme | NeedsVeinMetadata | NeedsGas))
            galaxy->ensureAllPlanets();
        if (requirements & NeedsGas) galaxy->ensureAllGas();
        std::vector<OwnedOutputRecord> ownedOutput;
        formatOutput(galaxy, ownedOutput);
        if (legacyFilterLock.owns_lock()) legacyFilterLock.unlock();
        {
            std::unique_lock lk(mutex2);
            found.fetch_add(1, std::memory_order_relaxed);
            runOutput(galaxy);
        }
        outputWriter->enqueue(fmt::format("{},{}\n", seed, galaxy->starCount),
                              std::move(ownedOutput));
        galaxy->release();
    }
    finishPluginThread();
    dspugen::util::instrumentation::mergeThread();
    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
}

static void pose(std::uint32_t workerIndex) {
    dspugen::util::instrumentation::resetThread();
    std::vector<dspugen::VectorLF3> poses;
    initPluginThread(workerIndex);
    int blockCurrent = 0;
    int blockEnd = 0;
    while (true) {
        int seed;
        if (!getNextSeed(seed, blockCurrent, blockEnd)) break;
        dspugen::Galaxy::GeneratePoses(dspugen::DefaultAlgoVersion, seed, starCount, poses);
        runPoseFilters(seed, starCount, poses);
        if (seed % 500000 == 0) {
            fmt::print(std::cerr, "Processed to: {},{}. {}ms elapsed.\n", seed, starCount, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *startTime).count());
        }
    }
    finishPluginThread();
    dspugen::util::instrumentation::mergeThread();
}

void addSeedByString(const std::string &buf, int stars = 64) {
    auto pos = buf.find('-');
    char *end = nullptr;
    int from = static_cast<int>(std::strtol(buf.c_str(), &end, 10));
    if (pos != std::string::npos && *end != '-' && *end != ',' && *end != '\0') {
        from = -1;
    }
    end = nullptr;
    int to = pos != std::string::npos ? static_cast<int>(std::strtol(buf.c_str() + pos + 1, &end, 10)) : from;
    if (end != nullptr && *end != ',' && *end != '\0') {
        to = -1;
    }
    if (from == -1 && to == -1) {
        return;
    }
    if (to >= from) {
        pos = buf.find(',');
        int starsTo = 0;
        if (pos != std::string::npos) {
            stars = static_cast<int>(std::strtol(buf.c_str() + pos + 1, nullptr, 10));
            auto pos2 = buf.find('-', pos + 1);
            if (pos2 != std::string::npos) {
                starsTo = static_cast<int>(std::strtol(buf.c_str() + pos2 + 1, nullptr, 10));
            }
        }
        if (starsTo > stars) {
            while (stars <= starsTo) {
                seedsToCheckMap[stars].emplace_back(from, to + 1);
                ++stars;
            }
        } else {
            seedsToCheckMap[stars].emplace_back(from, to + 1);
        }
    }
}

void readFromInputFile(const std::string &filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        fmt::print(std::cerr, "Unable to open {}!\n", filename);
        return;
    }
    while (!ifs.eof()) {
        std::string buf;
        std::getline(ifs, buf);
        while (buf.back() == '\r' || buf.back() == '\n') {
            buf.pop_back();
        }
        if (!buf.empty()) {
            addSeedByString(buf);
        }
    }
    ifs.close();
}

void sortSeeds() {
    for (auto &p: seedsToCheckMap) {
        auto &seeds = p.second;
        std::sort(seeds.begin(), seeds.end());
        auto sz = seeds.size();
        for (size_t i = 1; i < sz;) {
            auto last = seeds[i-1].second;
            if (last > seeds[i].first) {
                if (last < seeds[i].second) {
                    seeds[i-1].second = seeds[i].second;
                }
                seeds.erase(seeds.begin() + i);
                --sz;
            } else {
                ++i;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    const option longOptions[] = {
        /* input options */
        {"threads", required_argument, nullptr, 't'},
        {"input", required_argument, nullptr, 'i'},
        /* output options */
        {"output", required_argument, nullptr, 'o'},
        {"birth", no_argument, nullptr, 'b'},
        {"planets", no_argument, nullptr, 'p'},
        {"names", no_argument, nullptr, 'n'},
        {nullptr},
    };
    char opt;
    std::string inputFilename;
    std::string seedFilename = "seeds.csv";
    int threadCount = 0;
    while ((opt = getopt_long(argc, argv, ":t:i:o:bpPZn", longOptions, nullptr)) != -1) {
        switch (opt) {
        case ':':
            fmt::print(std::cerr, "mssing argument for {}\n", static_cast<char>(optopt));
            return -1;
        case '?':
            fmt::print(std::cerr, "bad arument: {}\n", static_cast<char>(optopt));
            return -1;
        case 'n':
            dspugen::settings.genName = true;
            break;
        case 'i':
            inputFilename = fmt::format("{}", optarg);
            break;
        case 'p':
            dspugen::settings.hasPlanets = true;
            break;
        case 'P':
            poseOnly = true;
            break;
        case 'Z':
            dspugen::settings.noPosition = true;
            break;
        case 'b':
            dspugen::settings.birthOnly = true;
            break;
        case 'o':
            seedFilename = fmt::format("{}", optarg);
            break;
        case 't':
            threadCount = std::stoi(optarg);
            break;
        default:
            break;
        }
    }
    if (optind >= argc && inputFilename.empty()) {
        fmt::print(std::cerr, "Usage: DSPSeedCalc [-t threads] [-n] [-i filename] [-b] [-p] [-P] [-o seeds.csv] [ranges...]\n");
        fmt::print(std::cerr, "          Ranges format: a-b[,starCount]. starCount is 64 by default, can be range.   e.g. 0-1000 / 333-666,32\n");
        fmt::print(std::cerr, "      -t  Threads to use, 0 for default, which means (logic CPU threads - 1)\n");
        fmt::print(std::cerr, "      -n  Generate names for stars(which will reduce calculation speed)\n");
        fmt::print(std::cerr, "      -b  Generate only birth star\n");
        fmt::print(std::cerr, "      -p  Generate planet info for plugins use\n");
        fmt::print(std::cerr, "      -P  Generate only poses, support only pose() filters\n");
        fmt::print(std::cerr, " Note: You need to supply either [filename] or [ranges...]\n");
        return -1;
    }
    loadFilters();
    auto requirements = prePlanetRequirements() | filterRequirements() |
                        outputRequirements();
    if (requirements & NeedsNames) dspugen::settings.genName = true;
    useBlockScheduling = pluginsAllowBlockScheduling();
    for (auto oind = optind; oind < argc; oind++) {
        addSeedByString(argv[oind]);
    }
    if (!inputFilename.empty()) {
        readFromInputFile(inputFilename);
    }
    sortSeeds();
    dspugen::loadProtoSets();
/*
    if (hasPlanets) {
        output = std::ofstream(planetFilename);
        fmt::print(output[0],
                   "Seed,Star Count,Planet Id,Around,Type,Tidal Locked,Iron,Copper,Silicium,Titanium,Stone,Coal,Oil,FireIce,Diamond,Fractal,Crysrub,Grat,Bamboo,UnipolarMagnet\n");
    }
*/
    outputWriter = new OutputWriter(seedFilename);
/*
    fmt::print(output[1], "Seed,Star Count,Star Id,Type,Distance,Luminosity,Name\n");
*/
    if (threadCount <= 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount > 1) --threadCount;
    }
    startTime = new std::chrono::time_point<std::chrono::steady_clock>(std::chrono::steady_clock::now());
    for (auto &p: seedsToCheckMap) {
        auto &seeds = p.second;
        starCount = p.first;
        totalSize = seeds.size();
        if (totalSize) {
            current = seeds[0].first;
            currMax = seeds[0].second;
        }
        currIndex = 0;
        seedsToCheck = &seeds;
        seedBlocks.clear();
        if (useBlockScheduling) {
            constexpr int blockSize = 64;
            for (const auto &range: seeds)
                for (int begin = range.first; begin < range.second; begin += blockSize)
                    seedBlocks.push_back({begin, std::min(begin + blockSize, range.second)});
            nextSeedBlock.store(0, std::memory_order_relaxed);
        }
        std::vector<std::thread> thr(threadCount);
        for (std::size_t i = 0; i < thr.size(); ++i) {
            if (poseOnly)
                thr[i] = std::thread(pose, static_cast<std::uint32_t>(i));
            else
                thr[i] = std::thread(calc, static_cast<std::uint32_t>(i));
        }
        for (auto &th: thr) {
            th.join();
        }
        mergePluginThreads();
    }
    auto duration = std::chrono::steady_clock::now() - *startTime;
    outputWriter->finish();
    delete outputWriter;
    unloadFilters();
    if (const char *instrumentationOutput = std::getenv("DSP_INSTRUMENTATION_OUTPUT"))
        dspugen::util::instrumentation::writeReport(instrumentationOutput);
    int count = 0;
    for (auto &sp: seedsToCheckMap) {
        for (auto &p: sp.second) {
            count += p.second - p.first;
        }
    }
    fmt::print(std::cerr, "Output file: {}\n", seedFilename);
    fmt::print(std::cerr, "============\n{}ms used, {} found from {} processed seeds.\n", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), found.load(std::memory_order_relaxed), count);
    delete startTime;
    return 0;
}
