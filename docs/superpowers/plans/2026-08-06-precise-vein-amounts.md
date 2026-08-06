# Precise Vein Amounts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in, CPU-exact path that generates per-planet, per-vein-type node counts and base amounts from the recorded Dyson Sphere Program 0.10.34.28529 algorithms, while leaving the default scan path unchanged.

**Architecture:** Keep the existing lightweight `veinSpot` metadata path. Add a lazy `PlanetRawData`/terrain/vein-summary layer that uses thread-local scratch buffers and stores only a sidecar summary. Gate the expensive path with `-V/--vein-amounts` and a versioned plugin ABI V2 requirement bit; do not add a host CSV or a GPU backend in this plan.

**Tech Stack:** C++17, CMake/Ninja, existing `DotNet35Random`, vendored `fmt` and `nlohmann::json`, ILSpy 10.1 static evidence, strict floating-point compilation (`-fno-fast-math -ffp-contract=off` or `/fp:strict`).

---

## File map

Create the following focused modules:

- `dspugen/terrain/unity_math.hh/.cc`: Unity-compatible `Vector2`, `Vector3`, `Quaternion`, and scalar helpers used only by terrain/vein code.
- `dspugen/terrain/simplex_noise.hh/.cc`: static port of the reference `SimplexNoise` implementation.
- `dspugen/terrain/planet_raw_data.hh/.cc`: precision-200 sphere grid, height/biome buffers, index map, `CalcVerts`, `QueryHeight`, `RawSnap`, and minimal vein-data sink.
- `dspugen/terrain/planet_algorithm.hh/.cc`: algorithm base class and dispatch context.
- `dspugen/terrain/planet_algorithm_0.cc` through `planet_algorithm_13.cc`: one reference-derived terrain implementation per algorithm ID.
- `dspugen/vein/vein_amounts.hh/.cc`: summary type, plan arrays, base/algorithm-specific vein generation, and amount accumulation.
- `tests/vein_amounts_test.cc`: standalone assertion executable for math, raw-data, summary, and pool-reset behavior.
- `tests/vein_amount_oracle.cc`: deterministic text oracle for selected seeds and summary fields.

Modify these existing files:

- `dspugen/CMakeLists.txt`: add all new library sources and the test/oracle targets when enabled.
- `dspugen/planet.hh/.cc`: append lazy summary state, restore only fields needed by terrain/birth/predict-pose code, and add `ensureVeinAmounts()`.
- `dspugen/galaxy.hh/.cc`: add idempotent `ensureAllVeinAmounts()` in normal star/planet order.
- `dspugen/protoset.hh/.cc`: validate required terrain/vein fields and expose any currently omitted fields required by the reference algorithms.
- `dspugen/settings.hh`: add the CLI enable flag state.
- `cli/filter.hh/.cc`: add `NeedsVeinAmounts`, `PluginAPIV2`, `init3`, V2 descriptors, and V2 host callbacks without changing V1 layouts.
- `cli/main.cc`: parse `-V/--vein-amounts`, reject `-P`/`-Z`, gate plugin requirements, and stage `ensureAllVeinAmounts()`.
- `tests/oracle.cc`: add a compact summary digest only after the new summary API is implemented; the existing default fixture must remain byte-identical.
- `CMakeLists.txt`: add `DSP_BUILD_TESTS` and wire the standalone test executable.

Do not modify the untracked user files `CLAUDE.md` and `filters/zz.cc`.

## Task 1: Add the red test harness and public summary contract

**Files:**

- Create: `tests/vein_amounts_test.cc`
- Modify: `CMakeLists.txt`
- Modify: `dspugen/CMakeLists.txt`
- Create: `dspugen/vein/vein_amounts.hh`
- Modify: `dspugen/planet.hh`

- [ ] **Step 1: Add a disabled-by-default test option and target.**

Add this to the root `CMakeLists.txt` after `DSP_BUILD_ORACLE`:

```cmake
option(DSP_BUILD_TESTS "Build deterministic generator unit tests" OFF)
```

Add this near the existing oracle target:

```cmake
if(DSP_BUILD_TESTS)
    add_executable(dspugen_vein_tests tests/vein_amounts_test.cc)
    target_link_libraries(dspugen_vein_tests PRIVATE dspugen)
    target_compile_features(dspugen_vein_tests PRIVATE cxx_std_17)
    set_target_properties(dspugen_vein_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
```

Add `vein/vein_amounts.hh` to the `dspugen` source list so the public summary type is visible to the test and plugin headers.

- [ ] **Step 2: Write the first failing assertions.**

Create a test executable with no external framework. The first main must fail because the summary API does not exist yet:

```cpp
#include "galaxy.hh"
#include "planet.hh"
#include "protoset.hh"
#include "settings.hh"

#include <cassert>
#include <cstddef>

int main() {
    dspugen::loadProtoSets();
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();
    auto *galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 0, 64);
    assert(galaxy != nullptr);
    assert(galaxy->stars[0]->planets[0]->veinAmountSummary() == nullptr);
    galaxy->ensureAllVeinAmounts();
    auto *summary = galaxy->stars[0]->planets[0]->veinAmountSummary();
    assert(summary != nullptr);
    assert(summary->nodeCount[0] == 0);
    galaxy->release();
    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
}
```

- [ ] **Step 3: Run the test and verify the expected compile failure.**

Run:

```powershell
cmake -S . -B cmake-build-vein-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DBUILD_VIEWER=OFF -DFMT_TEST=OFF -DDSP_BUILD_TESTS=ON
cmake --build cmake-build-vein-tests --target dspugen_vein_tests
```

Expected: compilation fails because `veinAmountSummary()` and `ensureAllVeinAmounts()` are not defined. Do not proceed if CMake or the test fails for an unrelated configuration error.

- [ ] **Step 4: Define the summary type and non-ready getter.**

In `dspugen/vein/vein_amounts.hh`, define:

```cpp
#pragma once

#include <array>
#include <cstdint>

namespace dspugen {

inline constexpr std::size_t VeinTypeCount = 15;

struct VeinAmountSummary {
    std::array<std::uint32_t, VeinTypeCount> nodeCount{};
    std::array<std::int64_t, VeinTypeCount> baseAmount{};
};

}
```

Append a private summary pointer and ready flag at the end of `Planet`, add `const VeinAmountSummary *veinAmountSummary() const`, and declare `void ensureVeinAmounts()`.

- [ ] **Step 5: Run the test and commit the contract.**

Run the same build. It must now compile but fail at the assertion that the summary is ready, proving the test reaches the missing implementation rather than testing a typo.

```powershell
cmake --build cmake-build-vein-tests --target dspugen_vein_tests
```

Commit only the contract and test harness:

```powershell
git add CMakeLists.txt dspugen/CMakeLists.txt dspugen/vein/vein_amounts.hh dspugen/planet.hh tests/vein_amounts_test.cc
git commit -m "test: add vein amount contract harness"
```

## Task 2: Port strict Unity math and `PlanetRawData`

**Files:**

- Create: `dspugen/terrain/unity_math.hh/.cc`
- Create: `dspugen/terrain/simplex_noise.hh/.cc`
- Create: `dspugen/terrain/planet_raw_data.hh/.cc`
- Modify: `dspugen/CMakeLists.txt`
- Modify: `tests/vein_amounts_test.cc`

- [ ] **Step 1: Add red tests for scalar and raw-data semantics.**

Add assertions for:

```cpp
assert(util::mathfRoundToInt(0.5f) == 0);
assert(util::mathfRoundToInt(1.5f) == 2);
assert(util::mathfRoundToInt(-0.5f) == 0);
PlanetRawData raw(200);
raw.CalcVerts();
assert(raw.dataLength() == 4 * 201 * 201);
```

Add a height fixture that writes a value above the water threshold and checks `QueryHeight` returns the same float after the reference `ushort` conversion.

- [ ] **Step 2: Port `Vector3`, `Quaternion`, and Unity scalar operations.**

Use the decompiled Unity math behavior as the source of truth. Implement `Normalize`, `normalized`, `sqrMagnitude`, `Cross`, `Dot`, `FromToRotation`, `AngleAxis`, and the exact float conversion points. Keep `Math.Cos`/`Math.Sin` in double precision and cast the result where the reference casts it.

Do not change existing `VectorLF3` or `util::maths.hh` formulas unless a new test proves a current helper is wrong; add new helpers in the terrain namespace to avoid altering the default galaxy path.

- [ ] **Step 3: Port `SimplexNoise`.**

Copy the reference `SimplexNoise` constructor, permutation table initialization, and noise methods from the recorded ILSpy output. Add fixed input tests for the seed and coordinate values used by the first terrain algorithm. Compare float bit patterns, not decimal text.

- [ ] **Step 4: Implement `PlanetRawData`.**

Implement the reference sequence:

```text
PlanetRawData(precision)
  → allocate heightData/modData
  → InitModData
  → CalcVerts
  → terrain writes height/biome
  → QueryHeight
```

Preserve index-map layout, `stride`, `substride`, `heightData` as `ushort`, biome clamping, and the raw height interpolation branch. Add a no-op `EraseVegetableAtPoint` and a summary-only `AddVeinData` sink; do not allocate model or collider state.

- [ ] **Step 5: Run the raw-data tests and commit.**

Run:

```powershell
cmake --build cmake-build-vein-tests --target dspugen_vein_tests
cmake-build-vein-tests\bin\dspugen_vein_tests.exe
```

Expected: all scalar, vertex-count, conversion, and height-query assertions pass. Commit the math and raw-data modules:

```powershell
git add dspugen/terrain tests/vein_amounts_test.cc dspugen/CMakeLists.txt
git commit -m "feat: add strict terrain math and raw data"
```

## Task 3: Port the reference terrain algorithms

**Files:**

- Create: `dspugen/terrain/planet_algorithm.hh/.cc`
- Create: `dspugen/terrain/planet_algorithm_0.cc` through `planet_algorithm_13.cc`
- Modify: `dspugen/protoset.hh/.cc`
- Modify: `dspugen/CMakeLists.txt`
- Modify: `tests/vein_amounts_test.cc`

- [ ] **Step 1: Add dispatch and unsupported-algorithm tests.**

Add tests that select each prototype algorithm in `0–13`, generate a precision-200 raw data object, and assert that height data is populated. Add a separate test with algorithm `14` that expects a typed unsupported-algorithm result rather than silently generating empty data.

- [ ] **Step 2: Restore theme fields needed by terrain.**

Keep existing JSON keys and add validation in `loadProtoSets()` for `Algos`, `ModX`, `ModY`, `WaterHeight`, and `WaterItemId`. Reject a malformed row before any seed scan. Do not alter the committed prototype JSON.

- [ ] **Step 3: Implement the base algorithm context.**

Define a context carrying `Planet`, `Star`, `PlanetRawData`, `SimplexNoise`, `modX`, `modY`, and strict math helpers. Expose `generateTerrain(double modX, double modY)` and a dispatch function that returns an explicit `UnsupportedAlgorithm` result for IDs outside `0–13`.

- [ ] **Step 4: Port algorithms `0–6` one at a time.**

For each file, copy the method operation order from the corresponding ILSpy output. After each algorithm, run the focused test with a fixed seed and record the height-data digest. Do not refactor formulas across algorithm files.

- [ ] **Step 5: Port algorithms `7–10` one at a time.**

Keep the algorithm-specific fields and all `double` to `float` conversions. Add the `PredictPose` dependency as a pure local pose helper; do not call game code or start the game.

- [ ] **Step 6: Port algorithms `11–13` one at a time.**

Preserve the height layering used later by the specialized vein rules. Add focused tests for the thresholds used by algorithms `11`, `12`, and `13`.

- [ ] **Step 7: Run the terrain matrix and commit.**

Run:

```powershell
cmake --build cmake-build-vein-tests --target dspugen_vein_tests
cmake-build-vein-tests\bin\dspugen_vein_tests.exe
```

Expected: algorithms `0–13` produce stable non-empty or intentionally flat digests, algorithm `14` reports unsupported, and all prior raw-data tests remain green. Commit the terrain port:

```powershell
git add dspugen/terrain dspugen/protoset.hh dspugen/protoset.cc dspugen/CMakeLists.txt tests/vein_amounts_test.cc
git commit -m "feat: port reference terrain algorithms"
```

## Task 4: Implement shared vein planning and exact summary accumulation

**Files:**

- Modify: `dspugen/vein/vein_amounts.hh`
- Create: `dspugen/vein/vein_amounts.cc`
- Modify: `dspugen/planet.cc`
- Modify: `dspugen/star.hh/.cc` only if a read-only resource-coefficient helper is needed
- Modify: `dspugen/CMakeLists.txt`
- Modify: `tests/vein_amounts_test.cc`

- [ ] **Step 1: Add red tests for amount and node semantics.**

Add direct summary tests for:

```cpp
assert(roundToIntEven(0.5f) == 0);
assert(roundToIntEven(1.5f) == 2);
assert(util::mathfRoundToInt(100.0f * 0.5f * 1.1f) == 55);
assert(util::mathfRoundToInt(1.0f * 0.5f * 1.1f) == 1);
```

Add an integration fixture that checks every accepted node increments exactly one vein type and that a rejected underwater node increments neither count nor amount.

- [ ] **Step 2: Extract the existing lightweight plan without changing its random calls.**

Refactor the current `Planet::generateVeins()` body into a local `VeinPlan` builder that owns the 15-element spot/count/opacity arrays. Keep the six prefix `next()` calls, rare-vein rolls, special-star loops, and source operation order. Copy the resulting spot array back to `Planet::veinSpot` exactly as before.

- [ ] **Step 3: Implement the base generator.**

Port `PlanetAlgorithm.GenerateVeins` using a second `DotNet35Random`. Use the ordinary-resource branch only: do not apply `GameDesc.resourceMultiplier`, `oilAmountMultiplier`, rare-resource coefficient adjustment, or infinite-resource overwrite. Still execute the ordinary star and birth-planet coefficient rules needed to reproduce normal-mode placement and amount distribution.

Use this exact accumulation point:

```cpp
auto amount = util::mathfRoundToInt(
    static_cast<float>(random.next(minAmount, maxAmount)) * 1.1f);
if (amount < 1) amount = 1;
if (waterItemId == 0 || !(height < radius)) {
    summary.nodeCount[type] += 1;
    summary.baseAmount[type] += amount;
}
```

The actual implementation must preserve the reference order around model selection, position snapping, height query, and water filtering; the snippet only identifies the summary update point.

- [ ] **Step 4: Implement specialized vein overrides.**

Port algorithms `7`, `11`, `12`, and `13` separately. Preserve algorithm `7` birth bias and no ordinary water subtraction, algorithm `11` iron/copper and silicon/titanium height split, algorithm `12` fireice lower-height threshold, and algorithm `13` type `1–4` upper-height threshold.

- [ ] **Step 5: Add birth-point and pose helpers.**

Restore the minimal values consumed by `GenBirthPoints` and algorithm `7` `PredictPose(120)`. Reuse random values already consumed by `Planet::create()`; never add a new random call to the default path. Add tests for the birth planet’s first two resource types and the algorithm `7` bias direction.

- [ ] **Step 6: Run the vein tests and commit.**

Run:

```powershell
cmake --build cmake-build-vein-tests --target dspugen_vein_tests
cmake-build-vein-tests\bin\dspugen_vein_tests.exe
```

Expected: amount rounding, oil handling, water filtering, special algorithms, and birth fixtures pass. Commit:

```powershell
git add dspugen/vein dspugen/planet.cc dspugen/star.hh dspugen/star.cc dspugen/CMakeLists.txt tests/vein_amounts_test.cc
git commit -m "feat: generate exact vein summaries"
```

## Task 5: Integrate lazy summaries with Planet and Galaxy pools

**Files:**

- Modify: `dspugen/planet.hh/.cc`
- Modify: `dspugen/galaxy.hh/.cc`
- Modify: `tests/vein_amounts_test.cc`

- [ ] **Step 1: Add the pool-reset regression test.**

Generate seed A, call `ensureAllVeinAmounts()`, retain a copied summary, release the galaxy, generate seed B on the same worker, and assert:

```cpp
assert(seedBPlanet->veinAmountSummary() == nullptr);
```

After calling `ensureAllVeinAmounts()` for seed B, assert every array element is overwritten and no value equals an unrequested seed A value by pointer reuse.

- [ ] **Step 2: Implement `Planet::reset()` invalidation.**

Clear the ready flag, zero the summary arrays when the sidecar exists, clear terrain scratch ownership, and preserve the current `veinSpot` reset. The pool must not depend on a constructor running again.

- [ ] **Step 3: Implement `Planet::ensureVeinAmounts()`.**

Make it idempotent. For gas planets, mark a zero summary ready. For supported solid themes, construct or reuse precision-200 raw data, call terrain dispatch, then the summary generator, and mark ready only after successful completion.

- [ ] **Step 4: Implement `Galaxy::ensureAllVeinAmounts()`.**

Iterate the existing `stars` and each star’s `planets` in their stored order. Call `ensureAllPlanets()` first when topology is incomplete. Do not create partial planets out of prefix order; preserve `HabitableCount` behavior.

- [ ] **Step 5: Run pool and idempotence tests and commit.**

Run the test executable twice in the same process and verify the second run has no stale summary. Commit:

```powershell
git add dspugen/planet.hh dspugen/planet.cc dspugen/galaxy.hh dspugen/galaxy.cc tests/vein_amounts_test.cc
git commit -m "feat: add lazy galaxy vein summary lifecycle"
```

## Task 6: Add plugin ABI V2 and CLI gating

**Files:**

- Modify: `cli/filter.hh/.cc`
- Modify: `cli/main.cc`
- Modify: `dspugen/settings.hh`
- Create: `filters/vein_amount_v2_test.cc`
- Modify: `filters/CMakeLists.txt`
- Modify: `tests/vein_amounts_test.cc`

- [ ] **Step 1: Add red CLI argument tests.**

Run the current executable with the new arguments before implementation:

```powershell
cmake-build-vein-tests\bin\DSPSeedCalc.exe -V -P 0-0
cmake-build-vein-tests\bin\DSPSeedCalc.exe -V -Z 0-0
```

Expected: the old executable reports an unknown option. After the parser is implemented, these cases must return nonzero with a specific conflict message.

- [ ] **Step 2: Define V2 structs without changing V1.**

Append, do not reorder, the existing V1 declarations. Add:

```cpp
struct PluginAPIV2 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    void (*GenerateAllPlanets)(const dspugen::Galaxy *);
    void (*GeneratePlanetGas)(const dspugen::Planet *);
    void (*GenerateAllVeinAmounts)(const dspugen::Galaxy *);
    const dspugen::VeinAmountSummary *(*GetPlanetVeinAmountSummary)(const dspugen::Planet *);
};

using PluginInit3Func = const char *(FILTERAPI *)(PluginAPIV2 *, int *);
```

Add `NeedsVeinAmounts = 1ULL << 7U` after the existing requirement bits. V2 descriptors must contain `structSize`, `abiVersion`, the three requirement masks, and capabilities with the same field order as V1 plus the version marker.

- [ ] **Step 3: Implement V2 loading and safe fallback.**

In `loadFilters()`, detect `init3` before `init`/`init2`. Accept a V2 descriptor only when `abiVersion == 2` and `structSize >= sizeof(PluginDescriptorV2)`. Keep V1 loading unchanged. A plugin exporting only `init3` must be skipped by an old host because the old host never calls it.

Add host helpers `pluginsRequireVeinAmounts()` and `generateAllVeinAmounts()`; the latter must call the new Galaxy method and never expose a pointer to asynchronous output storage.

- [ ] **Step 4: Implement CLI parsing and requirement validation.**

Change `char opt` to `int opt`, add `{ "vein-amounts", no_argument, nullptr, 'V' }`, add `V` to the getopt string, and set `dspugen::settings.hasPlanets = true` plus a new `settings.enableVeinAmounts` flag. After parsing and loading filters:

```cpp
if (poseOnly && dspugen::settings.enableVeinAmounts) {
    fmt::print(std::cerr, "-V cannot be combined with -P\\n");
    return -1;
}
if (dspugen::settings.noPosition && dspugen::settings.enableVeinAmounts) {
    fmt::print(std::cerr, "-V cannot be combined with -Z\\n");
    return -1;
}
if (pluginsRequireVeinAmounts() && !dspugen::settings.enableVeinAmounts) {
    fmt::print(std::cerr, "a plugin requires -V/--vein-amounts\\n");
    return -1;
}
```

Update usage text while leaving `Seed,Star Count` output unchanged.

- [ ] **Step 5: Stage the new requirement in `calc()`.**

Add `NeedsVeinAmounts` to the staged requirement masks. When the bit is present, call `ensureAllVeinAmounts()` before the callback stage that requested it; do not call `ensureAllPlanets()` afterward for the same stage. Preserve the legacy serialized-filter lock behavior.

- [ ] **Step 6: Build a V2 smoke plugin and run it.**

The plugin must declare `NeedsVeinAmounts`, call `GetPlanetVeinAmountSummary()` from `planetFilter`, and reject a seed when iron `baseAmount` is zero. Build it as a shared library and run:

```powershell
cmake --build cmake-build-vein-tests --target DSPSeedCalc vein_amount_v2_test
cmake-build-vein-tests\bin\DSPSeedCalc.exe -t 1 -V -o vein-v2-smoke.csv 0-8,64
```

Expected: the plugin loads, the run completes, and the output contains only the normal two-column seed rows. Running without `-V` must fail before scanning with the explicit requirement message.

- [ ] **Step 7: Commit the ABI and CLI changes.**

```powershell
git add cli/filter.hh cli/filter.cc cli/main.cc dspugen/settings.hh filters/CMakeLists.txt filters/vein_amount_v2_test.cc tests/vein_amounts_test.cc
git commit -m "feat: gate exact vein summaries through CLI and ABI v2"
```

## Task 7: Add deterministic summary oracle and default-path regression checks

**Files:**

- Create: `tests/vein_amount_oracle.cc`
- Modify: `CMakeLists.txt`
- Modify: `tests/oracle.cc`
- Create: `tests/fixtures/vein-amounts-v1.txt`
- Create: `tools/verify_vein_amounts.py`

- [ ] **Step 1: Write the oracle before adding expected output.**

The oracle must initialize prototype sets and thread pools, generate a fixed matrix of seeds/star counts, call `ensureAllVeinAmounts()`, and print integer arrays in stable star/planet order. It must also print `veinSpot` so default and precise paths can be compared.

- [ ] **Step 2: Run the oracle once and record the fixture.**

When `DSP_BUILD_ORACLE` is enabled, add this target beside `dspugen_oracle`:

```cmake
add_executable(dspugen_vein_amount_oracle tests/vein_amount_oracle.cc)
target_link_libraries(dspugen_vein_amount_oracle PRIVATE dspugen)
target_compile_features(dspugen_vein_amount_oracle PRIVATE cxx_std_17)
set_target_properties(dspugen_vein_amount_oracle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

Use a matrix covering one seed for each discovered algorithm `0–13`, a birth planet, one white dwarf, one neutron star, one black hole, and at least one oil-bearing and water-covered planet. Record the exact build type, compiler, prototype SHA-256 values, and command alongside the fixture header.

- [ ] **Step 3: Add byte-comparison verification.**

Implement `tools/verify_vein_amounts.py` with the same behavior as `tools/verify_oracle.py`: run the oracle, compare bytes with `tests/fixtures/vein-amounts-v1.txt`, write `.actual` on mismatch, and return nonzero.

- [ ] **Step 4: Extend the existing default oracle digest.**

Only add optional summary output to `tests/oracle.cc` behind a compile-time or command-line branch. The existing no-argument output must remain byte-identical; regenerate neither `tests/fixtures/oracle-v1.txt` nor its expected hash unless an unrelated baseline change is proven.

- [ ] **Step 5: Run the regression matrix and commit.**

Run the default oracle and the new summary verifier in Debug. Expected: the existing fixture is unchanged and the new fixture matches. Commit:

```powershell
git add CMakeLists.txt tests/oracle.cc tests/vein_amount_oracle.cc tests/fixtures/vein-amounts-v1.txt tools/verify_vein_amounts.py
git commit -m "test: add deterministic vein amount oracle"
```

## Task 8: Verify all required build configurations and document GPU findings

**Files:**

- Modify: `docs/optimization-verification.md`
- Create: `docs/vein-amounts-gpu-research.md`

- [ ] **Step 1: Run GCC Debug with viewer disabled.**

Run:

```powershell
cmake --build cmake-build-gcc-debug-parity --target dspugen_oracle DSPSeedCalc dspugen_vein_tests
cmake-build-gcc-debug-parity\bin\dspugen_oracle.exe
cmake-build-gcc-debug-parity\bin\dspugen_vein_tests.exe
```

Expected: exit code `0`, unchanged default oracle output, and matching precise summary fixture.

- [ ] **Step 2: Run GCC Release and Clang Release.**

Run the corresponding build presets and repeat both oracle commands. Compare integer fields exactly and compare any floating terrain digests by IEEE-754 bits.

- [ ] **Step 3: Run the CLI smoke matrix.**

Use isolated `filters` directories and `-t 1`:

```powershell
cmake-build-gcc-debug-parity\bin\DSPSeedCalc.exe -t 1 0-0,64
cmake-build-gcc-debug-parity\bin\DSPSeedCalc.exe -t 1 -V -o vein-v2-smoke.csv 0-8,64
cmake-build-gcc-debug-parity\bin\DSPSeedCalc.exe -t 1 -V -P 0-0,64
cmake-build-gcc-debug-parity\bin\DSPSeedCalc.exe -t 1 -V -Z 0-0,64
```

Expected: the first two commands succeed; the last two fail before generation with the documented conflict messages.

- [ ] **Step 4: Record performance without claiming parity from throughput.**

Benchmark default and precise modes separately, recording compiler, build type, thread count, seed interval, star count, plugin set, prototype snapshot, wall time, and seeds/s. Do not enable fast-math, PGO, or GPU code for correctness measurements.

- [ ] **Step 5: Write the GPU research note.**

Record the static conclusion: terrain height calculations are the only plausible GPU batch, but FP behavior and transcendental functions prevent an authority role; serial RNG/rejection sampling keeps vein placement on CPU. State that no OpenCL/CUDA dependency is introduced and a future GPU experiment must compare against CPU output before being used.

- [ ] **Step 6: Update verification documentation and commit.**

Add the exact commands and observed results to `docs/optimization-verification.md`, add the GPU note, run `git diff --check`, and commit:

```powershell
git add docs/optimization-verification.md docs/vein-amounts-gpu-research.md
git commit -m "docs: record precise vein verification and GPU boundary"
```

## Final verification checklist

- [ ] `git diff --check` is clean.
- [ ] `DSPSeedCalc` and `dspugen` build in GCC Debug, GCC Release, and Clang Release with C++17 and viewer disabled.
- [ ] Existing default oracle fixture is unchanged.
- [ ] Precise summary fixture matches in Debug and Release.
- [ ] V1 plugins still load; V2 plugin requires `-V` and reads summaries.
- [ ] Pool reuse clears unrequested summaries.
- [ ] Unsupported algorithm `14` fails explicitly.
- [ ] No game process was started and no debugger was attached.
- [ ] GPU is documented as non-authoritative and no GPU dependency is added.
