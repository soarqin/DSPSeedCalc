#include "filter.hh"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

// Bit-level digest of every generated star and planet field, reported at unload.
// Two builds that agree on the digest over a large seed range agree on every field's
// bit pattern, which covers far more ground than the fixed seeds in tests/oracle.cc.
// The per-seed value is folded in with xor so the result is independent of thread
// scheduling and seed order.
static std::atomic<std::uint64_t> digest;
static std::atomic<std::uint64_t> starCount;
static std::atomic<std::uint64_t> planetCount;

namespace {

struct Hasher {
    std::uint64_t value = 0xcbf29ce484222325ULL;

    void bytes(const void *data, std::size_t size) {
        const auto *cursor = static_cast<const unsigned char *>(data);
        for (std::size_t i = 0; i < size; ++i) {
            value ^= cursor[i];
            value *= 0x100000001b3ULL;
        }
    }

    template<typename T>
    void raw(const T &item) {
        static_assert(std::is_trivially_copyable<T>::value, "raw() needs a POD");
        bytes(&item, sizeof(item));
    }
};

} // namespace

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "benchmark galaxy digest";
}

extern "C" __declspec(dllexport) void FILTERAPI uninit() {
    std::fprintf(stderr, "galaxy_digest digest=%016llx stars=%llu planets=%llu\n",
                 static_cast<unsigned long long>(digest.load(std::memory_order_relaxed)),
                 static_cast<unsigned long long>(starCount.load(std::memory_order_relaxed)),
                 static_cast<unsigned long long>(planetCount.load(std::memory_order_relaxed)));
}

extern "C" __declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *galaxy, void *) {
    Hasher hasher;
    hasher.raw(galaxy->seed);
    hasher.raw(galaxy->starCount);
    hasher.raw(galaxy->birthStarId);
    hasher.raw(galaxy->HabitableCount);
    hasher.raw(galaxy->habitableTarget);
    std::uint64_t stars = 0;
    std::uint64_t planets = 0;
    for (const auto *star: galaxy->stars) {
        if (star == nullptr) continue;
        ++stars;
        hasher.raw(star->id);
        hasher.raw(star->index);
        hasher.raw(star->seed);
        hasher.raw(star->level);
        hasher.raw(star->type);
        hasher.raw(star->spectr);
        hasher.raw(star->mass);
        hasher.raw(star->lifetime);
        hasher.raw(star->age);
        hasher.raw(star->temperature);
        hasher.raw(star->luminosity);
        hasher.raw(star->radius);
        hasher.raw(star->habitableRadius);
        hasher.raw(star->lightBalanceRadius);
        hasher.raw(star->orbitScaler);
        hasher.raw(star->dysonRadius);
        hasher.raw(star->position.x);
        hasher.raw(star->position.y);
        hasher.raw(star->position.z);
        hasher.raw(star->color);
        hasher.raw(star->classFactor);
        hasher.raw(star->resourceCoef);
        hasher.bytes(star->name.data(), star->name.size());
        for (const auto *planet: star->planets) {
            if (planet == nullptr) continue;
            ++planets;
            hasher.raw(planet->id);
            hasher.raw(planet->index);
            hasher.raw(planet->number);
            hasher.raw(planet->seed);
            hasher.raw(planet->type);
            hasher.raw(planet->singularity);
            hasher.raw(planet->theme);
            hasher.raw(planet->algoId);
            hasher.raw(planet->orbitAround);
            hasher.raw(planet->orbitIndex);
            hasher.raw(planet->orbitRadius);
            hasher.raw(planet->radius);
            hasher.raw(planet->scale);
            hasher.raw(planet->habitableBias);
            hasher.raw(planet->sunDistance);
            hasher.raw(planet->temperatureBias);
            hasher.raw(planet->luminosity);
            hasher.raw(planet->themeSeed);
            hasher.bytes(planet->veinSpot, sizeof(planet->veinSpot));
            for (auto item: planet->gasItems) hasher.raw(item);
            for (auto speed: planet->gasSpeeds) hasher.raw(speed);
        }
    }
    digest.fetch_xor(hasher.value, std::memory_order_relaxed);
    starCount.fetch_add(stars, std::memory_order_relaxed);
    planetCount.fetch_add(planets, std::memory_order_relaxed);
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->filterRequirements = NeedsPositions | NeedsStars | NeedsNames |
                                     NeedsPlanetTopology | NeedsPlanetTheme |
                                     NeedsGas | NeedsVeinMetadata;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
