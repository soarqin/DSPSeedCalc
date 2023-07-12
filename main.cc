#include "dsp/galaxy.hh"
#include "dsp/protoset.hh"
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
static bool birthOnly = true;
static bool hasStars = false;
static std::ofstream output[2];
static int found = 0;

void outputFunc(const Star *star) {
    auto seed = star->galaxy->seed;
    if (hasStars) {
        fmt::print(output[1],
                   "{},{},{},{:.3f},{:.3f},{}\n",
                   seed,
                   star->id,
                   star->typeName(),
                   star->position.magnitude(),
                   pow(star->luminosity, 0.33000001311302185),
                   star->name);
    }
    for (auto &planet: star->planets) {
        fmt::print(output[0],
                   "{},{},{},{},{}",
                   seed,
                   planet->id,
                   planet->orbitAround,
                   planet->theme,
                   planet->singularity & EPlanetSingularity::TidalLocked ? 1 : 0);
        for (int i = 1; i < 15; ++i) {
            fmt::print(output[0], ",{}", planet->veinSpot[i]);
        }
        fmt::print(output[0], "\n");
    }
};

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
        auto galaxy = Galaxy::create(DefaultAlgoVersion, seed, starCount, genName, birthOnly);
        if (!runFilters(galaxy)) {
            galaxy->release();
            continue;
        }
        {
            std::unique_lock lk(mutex2);
            ++found;
            if (!runOutput(galaxy)) {
                if (birthOnly) {
                    const auto *star = galaxy->starById(galaxy->birthStarId);
                    outputFunc(star);
                } else {
                    for (const auto *star: galaxy->stars) {
                        outputFunc(star);
                    }
                }
            }
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
        if (pos != std::string::npos) {
            stars = (int)std::strtol(buf.c_str() + pos + 1, nullptr, 10);
        }
        seedsToCheckMap[stars].emplace_back(from, to + 1);
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
        currIndex = 0;
    }
}

int main(int argc, char *argv[]) {
    const option longOptions[] = {
        /* input options */
        {"input", required_argument, nullptr, 'i'},
        {"range", required_argument, nullptr, 'r'},
        /* output options */
        {"names", no_argument, nullptr, 'n'},
        {"birth", required_argument, nullptr, 'b'},
        {"stars", required_argument, nullptr, 's'},
        {"planets", required_argument, nullptr, 'p'},
        {nullptr},
    };
    char opt;
    std::string inputFilename;
    std::string starFilename;
    std::string planetFilename;
    while ((opt = getopt_long(argc, argv, ":i:r:b:s:p:n", longOptions, nullptr)) != -1) {
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
            if (!planetFilename.empty()) {
                fmt::print(std::cerr, "You cannot supply both -p and -b\n");
                return -1;
            }
            planetFilename = fmt::format("{}", optarg);
            birthOnly = false;
            break;
        case 'b':
            if (!planetFilename.empty()) {
                fmt::print(std::cerr, "You cannot supply both -p and -b\n");
                return -1;
            }
            planetFilename = fmt::format("{}", optarg);
            birthOnly = true;
            break;
        case 's':
            starFilename = fmt::format("{}", optarg);
            hasStars = true;
            break;
        default:
            break;
        }
    }
    if (optind >= argc && inputFilename.empty()) {
        fmt::print(std::cerr, "Usage: DSPSeedCalc [-n] [-i filename] [-b birth.csv|-p planets.csv] [-s stars.csv] [ranges...]\n");
        fmt::print(std::cerr, "         Ranges format: a-b[,starCount]. starCount is 64 by default.   e.g. 0-1000 / 333-666,32\n");
        fmt::print(std::cerr, "      -n Generate names for stars(which will reduce calculation speed)\n");
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
    loadProtoSets();
    if (planetFilename.empty()) {
        planetFilename = "birth.csv";
    }
    output[0] = std::ofstream(planetFilename);
    fmt::print(output[0], "Seed,Planet Id,Around,Type,Tidal Locked,Iron,Copper,Silicium,Titanium,Stone,Coal,Oil,FireIce,Diamond,Fractal,Crysrub,Grat,Bamboo,UnipolarMagnet\n");
    if (hasStars) {
        output[1] = std::ofstream(starFilename);
        fmt::print(output[1], "Seed,Star Id,Type,Distance,Luminosity,Name\n");
    }
    auto startTime = std::chrono::steady_clock::now();
    auto threadCount = std::thread::hardware_concurrency();
    if (threadCount > 1) --threadCount;
    for (auto &p: seedsToCheckMap) {
        auto &seeds = p.second;
        starCount = p.first;
        totalSize = seeds.size();
        if (totalSize) {
            current = seeds[0].first;
            currMax = seeds[0].second;
        }
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
    if (hasStars) {
        output[1].close();
    }
    output[0].close();
    int count = 0;
    for (auto &sp: seedsToCheckMap) {
        for (auto &p: sp.second) {
            count += p.second - p.first;
        }
    }
    fmt::print(std::cout, "Output files\n============\n  Stars: {}\n  Planets: {}\n", starFilename, planetFilename);
    fmt::print(std::cout, "============\n{}ms used, {} found from {} processed seeds.\n", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), found, count);
    return 0;
}
