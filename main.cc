#include "dsp/galaxy.hh"
#include "dsp/protoset.hh"

#include <fmt/format.h>
#include <getopt.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cmath>

static std::mutex mutex1, mutex2;
static std::vector<std::pair<int, int>> seedsToCheck;
static size_t currIndex = 0, totalSize = 0;
static int current = -1, currMax = -1;

static FILE *output[2];

static void calc() {
    while (true) {
        int seed;
        {
            std::unique_lock lk(mutex1);
            if (current >= currMax) {
                if (++currIndex >= totalSize) {
                    break;
                }
                current = seedsToCheck[currIndex].first;
                currMax = seedsToCheck[currIndex].second;
                seed = current++;
            } else {
                seed = current++;
            }
        }
        auto galaxy = Galaxy::create(DefaultAlgoVersion, seed, 64);
        int countByStarType[11] = {};
        double nearest[4] = {
            1000., 1000., 1000., 1000.,
        };
        float nearestOLuminosity = 0.f;
        double nearestVeins[16] = {
            1000., 1000., 1000., 1000.,
            1000., 1000., 1000., 1000.,
            1000., 1000., 1000., 1000.,
            1000., 1000., 1000., 1000.,
        };
        for (auto &star: galaxy->stars) {
            int typeIndex = -1;
            if (star->spectr != ESpectrType::X) {
                countByStarType[(int)star->spectr]++;
            }
            switch (star->type) {
            case EStarType::GiantStar:
                switch (star->spectr) {
                case ESpectrType::K:
                case ESpectrType::M:
                    countByStarType[7]++;
                    break;
                case ESpectrType::G:
                case ESpectrType::F:countByStarType[8]++;
                    break;
                case ESpectrType::A:countByStarType[9]++;
                    break;
                default:countByStarType[10]++;
                    break;
                }
                break;
            case EStarType::NeutronStar:
                typeIndex = 1;
                break;
            case EStarType::WhiteDwarf:
                typeIndex = 2;
                break;
            case EStarType::BlackHole:
                typeIndex = 3;
                break;
            default:
                if (star->spectr == ESpectrType::O) {
                    typeIndex = 0;
                }
                break;
            }
            auto distance = star->position.magnitude();
            for (auto &planet: star->planets) {
                if (planet->theme == 13) {
                    if (distance < nearestVeins[15]) {
                        nearestVeins[15] = distance;
                    }
                }
                for (int i = 9; i < 15; ++i) {
                    if (planet->veinSpot[i] > 0 && distance < nearestVeins[i]) {
                        nearestVeins[i] = distance;
                    }
                }
            }
            if (typeIndex < 0) { continue; }
            if (distance < nearest[typeIndex]) {
                nearest[typeIndex] = distance;
                if (typeIndex == 0) {
                    nearestOLuminosity = star->luminosity;
                }
            }
        }
        std::unique_lock lk(mutex2);
        fmt::print(output[0], "{}", seed);
        for (auto &n: countByStarType) {
            fmt::print(output[0], ",{}", n);
        }
        fmt::print(output[0], ",{:.3f}", pow(nearestOLuminosity, 0.33000001311302185));
        for (auto &n: nearest) {
            fmt::print(output[0], ",{:.3f}", n);
        }
        for (int i = 9; i < 16; ++i) {
            fmt::print(output[0], ",{:.3f}", nearestVeins[i]);
        }
        fmt::print(output[0], "\n");
        const auto *star = galaxy->starById(galaxy->birthStarId);
        for (auto &planet: star->planets) {
            fmt::print(output[1], "{},{},{},{},{}", seed, planet->id, planet->orbitAround, planet->theme, planet->singularity & EPlanetSingularity::TidalLocked ? 1 : 0);
            for (int i = 1; i < 15; ++i) {
                fmt::print(output[1], ",{}", planet->veinSpot[i]);
            }
            fmt::print(output[1], "\n");
        }
    }
}

void readFromInputFile(const char *filename) {
    FILE *fin = fopen(filename, "rt");
    if (fin == nullptr) {
        fprintf(stderr, "Unable to open %s!\n", filename);
        return;
    }
    char buf[64];
    while (!feof(fin)) {
        if (!fgets(buf, 64, fin) || buf[0] == 0) { break; }
        auto *pos = strchr(buf, '-');
        int from = (int)strtol(buf, nullptr, 0);
        int to = pos != nullptr ? (int)strtol(pos + 1, nullptr, 0) : from;
        if (to >= from) {
            seedsToCheck.emplace_back(from, to + 1);
        }
    }
    fclose(fin);
    currIndex = 0;
    totalSize = seedsToCheck.size();
    if (totalSize) {
        current = seedsToCheck[0].first;
        currMax = seedsToCheck[0].second;
    }
}

int main(int argc, char *argv[]) {
    const option longOptions[] = {
        /* input options */
        {"input", required_argument, nullptr, 'i'},
        {"range", required_argument, nullptr, 'r'},
        /* output options */
        {"stars", required_argument, nullptr, 's'},
        {"birth", required_argument, nullptr, 'b'},
        {nullptr},
    };
    char opt;
    char inputFilename[512] = "";
    char file1[512] = "stars.csv";
    char file2[512] = "birth.csv";
    while ((opt = getopt_long(argc, argv, ":i:r:s:b:", longOptions, nullptr)) != -1) {
        switch (opt) {
        case ':':
            fmt::print(stderr, "mssing argument for {}\n", optopt);
            return -1;
        case '?':
            fmt::print(stderr, "bad arument: {}\n", optopt);
            return -1;
        case 'i':
            snprintf(inputFilename, 512, "%s", optarg);
            break;
        case 's':
            snprintf(file1, 512, "%s", optarg);
            break;
        case 'b':
            snprintf(file2, 512, "%s", optarg);
            break;
        default:
            break;
        }
    }
    readFromInputFile(inputFilename);
    loadProtoSets();
    output[0] = fopen(file1, "wt");
    output[1] = fopen(file2, "wt");
    fmt::print(output[0], "Seed,M Stars,K Stars,G Stars,F Stars,A Stars,B Stars,O Stars,Red Giants,Yellow Giants,White Giants,Blue Giants,Nearest O Luminosity,Nearest O Star,Nearest Neutron,Nearest White Dwarf,Nearest Black Hole,Nearest Kimberlite vein,Fractal silicon vein,Organic crystal vein,Optical grating crystal vein,Spiniform stalagmite crystal vein,Unipolar magnet vein,Sulfuric acid ocean\n");
    fmt::print(output[1], "Seed,Planet Id,Around,Type,Tidal Locked,Iron,Copper,Silicium,Titanium,Stone,Coal,Oil,FireIce,Diamond,Fractal,Crysrub,Grat,Bamboo,UnipolarMagnet\n");
    auto startTime = std::chrono::steady_clock::now();
    auto threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> thr(threadCount);
    for (auto &th: thr) {
        th = std::thread(calc);
    }
    for (auto &th: thr) {
        th.join();
    }
    auto duration = std::chrono::steady_clock::now() - startTime;
    fclose(output[1]);
    fclose(output[0]);
    fmt::print(stdout, "Planet types\n============\n");
    for (int i = 1; i < 22; ++i) {
        auto *theme = themeProtoSet.select(i);
        if (!theme) { continue; }
        fmt::print(stdout, "{:2}: {}\n", i, translate(theme->displayName));
    }

    fmt::print(stderr, "\nOutput files\n============\n  {}: Star stats\n  {}: Birth star planet stats\n", file1, file2);
    fmt::print(stderr, "============\n{}ms used.\n", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    return 0;
}
