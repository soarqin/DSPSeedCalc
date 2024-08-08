#include "galaxy.hh"
#include "protoset.hh"
#include "filter.hh"

#include <fmt/ostream.h>
#include <fmt/format.h>
#include <getopt.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <fstream>
#include <iostream>
#include <map>
#include <cmath>

static std::mutex mutex1, mutex2;
static std::map<int, std::vector<std::pair<int, int>>> seedsToCheckMap;
static std::vector<std::pair<int, int>> *seedsToCheck = nullptr;
static size_t currIndex = 0, totalSize = 0;
static int current = -1, currMax = -1, starCount = 64;

static bool genName = false;
bool hasPlanets = false;
bool birthOnly = false;
static std::ofstream output;
static int found = 0;
static std::chrono::time_point<std::chrono::steady_clock> startTime;

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

static void calc() {
    while (true) {
        int seed;
        {
            std::unique_lock lk(mutex1);
            if (current >= currMax) {
                if (++currIndex >= totalSize) {
                    break;
                }
                current = (*seedsToCheck)[currIndex].first;
                currMax = (*seedsToCheck)[currIndex].second;
                seed = current++;
            } else {
                seed = current++;
            }
        }
        if (seed % 500000 == 0) {
            fmt::print(std::cout, "Processed to: {},{}. Currently found: {}. {}ms elapsed.\n", seed, starCount, found, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());
        }
        auto galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount, genName, hasPlanets, birthOnly);
        if (!runFilters(galaxy)) {
            galaxy->release();
            continue;
        }
        {
            std::unique_lock lk(mutex2);
            ++found;
            runOutput(galaxy);
            fmt::print(output, "{},{}\n", seed, galaxy->starCount);
        }
        galaxy->release();
    }
}

void addSeedByString(const std::string &buf, int stars = 64) {
    auto pos = buf.find('-');
    int from = (int)std::strtol(buf.c_str(), nullptr, 10);
    int to = pos != std::string::npos ? (int)std::strtol(buf.c_str() + pos + 1, nullptr, 10) : from;
    if (from == 0 && to == 0) {
        return;
    }
    if (to >= from) {
        pos = buf.find(',');
        int starsTo = 0;
        if (pos != std::string::npos) {
            stars = (int)std::strtol(buf.c_str() + pos + 1, nullptr, 10);
            auto pos2 = buf.find('-', pos + 1);
            if (pos2 != std::string::npos) {
                starsTo = (int)std::strtol(buf.c_str() + pos2 + 1, nullptr, 10);
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
    while ((opt = getopt_long(argc, argv, ":t:i:o:bpn", longOptions, nullptr)) != -1) {
        switch (opt) {
        case ':':
            fmt::print(std::cerr, "mssing argument for {}\n", (char)optopt);
            return -1;
        case '?':
            fmt::print(std::cerr, "bad arument: {}\n", (char)optopt);
            return -1;
        case 'n':
            genName = true;
            break;
        case 'i':
            inputFilename = fmt::format("{}", optarg);
            break;
        case 'p':
            hasPlanets = true;
            break;
        case 'b':
            birthOnly = true;
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
        fmt::print(std::cerr, "Usage: DSPSeedCalc [-t threads] [-n] [-i filename] [-b] [-p] [-o seeds.csv] [ranges...]\n");
        fmt::print(std::cerr, "          Ranges format: a-b[,starCount]. starCount is 64 by default, can be range.   e.g. 0-1000 / 333-666,32\n");
        fmt::print(std::cerr, "      -t  Threads to use, 0 for default, which means (logic CPU threads - 1)\n");
        fmt::print(std::cerr, "      -n  Generate names for stars(which will reduce calculation speed)\n");
        fmt::print(std::cerr, "      -b  Generate only birth star\n");
        fmt::print(std::cerr, "      -p  Generate planet info for plugins use\n");
        fmt::print(std::cerr, " Note: You need to supply either [filename] or [ranges...]\n");
        return -1;
    }
    loadFilters();
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
    output = std::ofstream(seedFilename);
    fmt::print(output, "Seed,Star Count\n");
/*
    fmt::print(output[1], "Seed,Star Count,Star Id,Type,Distance,Luminosity,Name\n");
*/
    if (threadCount <= 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount > 1) --threadCount;
    }
    startTime = std::chrono::steady_clock::now();
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
        std::vector<std::thread> thr(threadCount);
        for (auto &th: thr) {
            th = std::thread(calc);
        }
        for (auto &th: thr) {
            th.join();
        }
    }
    auto duration = std::chrono::steady_clock::now() - startTime;
    output.close();
    unloadFilters();
    int count = 0;
    for (auto &sp: seedsToCheckMap) {
        for (auto &p: sp.second) {
            count += p.second - p.first;
        }
    }
    fmt::print(std::cout, "Output file: {}\n", seedFilename);
    fmt::print(std::cout, "============\n{}ms used, {} found from {} processed seeds.\n", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), found, count);
    return 0;
}
