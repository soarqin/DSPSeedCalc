# DSPSeedCalc Contributor Guide

## Project Scope

DSPSeedCalc is a C++ port of Dyson Sphere Program's deterministic galaxy generator. Correctness means matching the target game's random-number consumption, floating-point conversion points, object ordering, and generated metadata for the same seed and star count.

The current reference installation is:

- Game: Dyson Sphere Program `0.10.34.xxxxx` (Steam build `25599610`)
- Unity: `2022.3.62f3c1`
- Script backend: Mono
- Galaxy algorithm: `20200403`
- Game path: `D:\Steam\steamapps\common\Dyson Sphere Program`
- Managed code: `DSPGAME_Data\Managed\Assembly-CSharp.dll`
- Steam build ID: `25599610`
- `Assembly-CSharp.dll` SHA-256: `6C122E5443E6843979B4064050DFCB5E0D75577A0B64F6AE4111290238B33C12`
- `Assembly-CSharp.dll` MVID: `7c753b2c-7be8-45e8-ae2a-51c600ac1211`
- Prototype snapshot: AssetStudio exports committed in `Prototypes/`
- Previous reference (superseded): SHA-256 `AE0BA95F75BD879A62AA4CE253B2AB78EAA4FB3C7C595F5E1FEE75EBE0E0EF85`, MVID `ece4a40e-5e73-43f4-a9f8-4e74970b5942`, Steam build `23109513`
- Oil generation rework: the current build allows up to `ceil(spot * 15 / 100)` underwater oil clusters (at least 1 m below sea level).  Each accepted underwater oil cluster decrements the allowance and adds one replacement attempt.  Oil nodes are always added even underwater.  `PlanetAlgorithm7` overrides `GenerateVeins` without this bookkeeping, so the underwater-oil allowance only applies to the shared base-class path (algorithms 1–6, 8–14).

Do not assume that behavior from an older Unity, Mono, or game build is still correct. Record a new reference version before changing parity-sensitive code.

## Repository Map

- `cli/main.cc`: CLI parsing and multi-threaded seed scanning.
- `cli/filter.cc`, `cli/filter.hh`: dynamic filter and output plugin host. `filter.hh` is the plugin ABI contract, exported to plugin targets through the `dsp_plugin_abi` INTERFACE target in `cli/CMakeLists.txt`.
- `dspugen/`: deterministic generator library.
- `dspugen/galaxy.cc`: star positions and galaxy composition.
- `dspugen/star.cc`: star properties and planetary-system topology.
- `dspugen/planet.cc`: planet metadata, type, theme, gas, and partial vein data.
- `dspugen/util/dotnet35random.*`: game-compatible legacy .NET random generator.
- `Prototypes/`: AssetStudio exports used at runtime. Treat these files as versioned game data.
- `filters/`: plugins; each `.cc` file becomes a shared library that links `dsp_plugin_abi` for `filter.hh` and the `dspugen` headers.
- `viewer/`: incomplete and out of scope unless a task explicitly includes it.
- `dspugen/fmt/`, `dspugen/nlohmann/`, `getopt/`, `dlfcn-win32/`: vendored code. Do not edit without a specific dependency task.

Terrain generation and full vein generation are currently out of scope for game-parity work unless explicitly requested.

## Build

The project requires C++17. Configure with an explicit standard rather than relying on the compiler default.

```powershell
cmake -S . -B cmake-build-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_STANDARD=17 `
  -DBUILD_VIEWER=OFF `
  -DFMT_TEST=OFF
cmake --build cmake-build-debug --target dspugen DSPSeedCalc
```

Release build:

```powershell
cmake -S . -B cmake-build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_STANDARD=17 `
  -DBUILD_VIEWER=OFF `
  -DFMT_TEST=OFF
cmake --build cmake-build-release --target dspugen DSPSeedCalc
```

For the installed MSYS2 Clang toolchain, prepend `C:\msys64\clang64\bin` to `PATH` before configuring. If the local installation uses a different MSYS2 root, use its corresponding `clang64\bin` directory.

Build one plugin by its filename stem, for example:

```powershell
cmake --build cmake-build-debug --target test2
```

There is no first-party CTest suite yet. Report the exact build, parity fixture, or smoke command that was run; do not summarize a successful build as "all tests passed."

## Running

The process resolves `Prototypes`, `filters`, and output files relative to its working directory. Use a prepared run directory containing only the intended plugins and the matching Prototype snapshot.

For deterministic checks:

- Use `-t 1` so output and plugin state are stable.
- Use `-p` when filters or checks require planets.
- Use `-n` only when checking names.
- Use `-P` only for star-position generation.
- Do not use `-Z` when checking positions, distances, or resource coefficients.

Example range syntax:

```powershell
cmake-build-debug\bin\DSPSeedCalc.exe -t 1 -p -n -o smoke.csv "0-0,64"
```

The plugin host loads every regular file in the working directory's `filters` folder. Isolate a plugin when validating it; multiple type-0 plugins combine as filters and may reject every seed.

## Game Comparison Workflow

Use ILSpy for this Mono build. Do not use an IL2CPP workflow unless the installed game backend changes.

```powershell
ilspycmd --disable-updatecheck --nested-directories -p `
  -o "$env:TEMP\dsp-decompile" `
  -r "D:\Steam\steamapps\common\Dyson Sphere Program\DSPGAME_Data\Managed" `
  "D:\Steam\steamapps\common\Dyson Sphere Program\DSPGAME_Data\Managed\Assembly-CSharp.dll"
```

Before using decompiled code as an oracle, record:

- game version and Steam build ID;
- Unity version and Mono/IL2CPP backend;
- `Assembly-CSharp.dll` SHA-256 and MVID;
- `UniverseGen.algoVersion`;
- Prototype snapshot;
- seed, star count, and compared fields.

For viewer and `UIVirtualStarmap` parity work, read `docs/viewer-parity-evidence.md` before starting research. Treat entries marked established or ruled out as closed for the recorded game build. Do not repeat asset extraction, managed-code tracing, shader disassembly, or Unity native-code analysis unless at least one of these conditions applies:

- the reference game or Unity build changed;
- a cited artifact is missing or cannot reproduce the recorded conclusion;
- new runtime evidence contradicts the recorded conclusion;
- an unresolved question explicitly requires additional data from that area.

When reopening an area, record the reason, the new evidence, and the delta from the existing conclusion before doing the work. Afterward, update the ledger with the result, exact evidence location, implementation status, and remaining open question. Screenshots may classify symptoms and validate results, but must not be used to infer constants already available from code, assets, shaders, or native implementation.

Verify parity in this order:

1. `DotNet35Random` output and derived seeds.
2. Star positions and their ordering.
3. Galaxy stellar-type quotas.
4. Star intermediate and final fields.
5. Planet count, seeds, orbit hierarchy, and gas-giant choices.
6. Planet type, singularities, theme, and gas data.
7. Names and plugin-visible derived values.

## Parity-Sensitive Rules

- Preserve every random call, including apparently unused calls. A removed call can shift all later results.
- Preserve source operation order and each `double` to `float` conversion point from C# or IL.
- Mono evaluates `float32` arithmetic in a wider internal representation. On x86-64 the evaluation stack holds `float32` as R8: `ldfld float32` widens to double, the arithmetic runs in double, and `conv.r4` narrows only when the value is stored to a `float32` location — a field, a local, an argument, or a return value. So port a float expression by evaluating it in `double` and narrowing once at each place the reference stores to `float32`, not once per operation. `double d = floatA * floatB;` is a double multiply.
- This applies to `Mathf` too: a `Mathf` call narrows its arguments and its result, but the expressions that build those arguments are still evaluated in double. Treating every intermediate as single precision produces 1-ULP errors that survive into the output.
- A corollary: a helper returning `float32` narrows at its return, so calling it differs from inlining its body. `Vector4.Magnitude` is `(float)Math.Sqrt(Dot(a, a))` and `Dot` returns `float32`, so the sum of squares is narrowed before the square root; `Vector3.Magnitude` writes the sum inline and keeps it in double. Match the exact overload the reference calls — same-named `Vector2`/`Vector3`/`Vector4` operations are not interchangeable. Both shapes appear in this codebase and each is wrong in the other's place.
- When you rewrite a C# float expression to evaluate in `double`, every `Nf` literal in it must become `dbl(Nf)`, never the C++ double literal `N`. Mono widens `0.2f` to `0.20000000298023224`, not to `0.2`. Only binary-exact values (`0.5f`, `1f`, `1.5f`, `2.5f`, `3f`, integers) are the same either way. This single mistake accounted for most of the star and planet field mismatches, and it is doubly dangerous because it makes a correct narrowing fix look like a regression.
- `std::pow(float, float)` resolves to `powf`, which evaluates in single precision; `Mathf.Pow` is `(float)Math.Pow(double, double)`, which evaluates in double and narrows once. Use `util::mathfPow` / `mathfLog` / `mathfSqrt`. The two agree on all but roughly one value in 60,000, so this survives small samples.
- Confirm a chosen float shape against many planets, not one. These differences only surface where a value sits near a truncation boundary, so a single planet can be matched by the wrong rule: on this codebase's algorithm 4, a two-planet sample endorsed a rule that a fifty-planet sample then refuted.
- Do not diagnose a float field by rebuilding and watching its mismatch count: that count also moves when the field's *inputs* are wrong, so it cannot separate a bad formula from bad upstream. Recompute the field offline from the reference dump's own values of its inputs and test candidate semantics against the reference's own value of the field — upstream error is then zero by construction. `tools/parity/probe_fields.py` does this; extend it rather than starting a build-and-measure loop.
- Treat `System.Math` operations as double precision and cast only where the game casts.
- Check midpoint behavior before replacing `Mathf.Round` or `Mathf.RoundToInt`; do not assume `std::round` is equivalent.
- Preserve long decimal constants emitted by decompilation when they represent widened `float` values.
- Do not enable fast-math or algebraic reassociation in parity-sensitive translation units.
- Avoid fused operations where the game performs separate rounded operations.
- Do not simplify temporary variables copied from the game until bit-level fixtures prove equivalence.
- Keep normal generation order. Habitable counts and theme de-duplication depend on earlier planets.
- The legacy `DotNet35Random` is part of the file format of a seed. Never replace it with `std::random` or modern `.NET Random`.
- Keep parity fixes separate from terrain, vein, combat, viewer, plugin ABI, and general refactoring changes.

When a field is intentionally omitted from the C++ model, continue consuming any random values that precede retained fields.

## Coding Conventions

- Match the existing C++ style: four-space indentation, braces on the same line, `.hh` headers, and lower-camel local names.
- Keep changes surgical. Do not reformat decompiler-shaped formulas while correcting them.
- Prefer fixed-width integer types where overflow or conversion behavior is part of parity.
- Add comments only for non-obvious parity constraints, especially required random calls or precision barriers.
- Initialize reused pool objects completely. `Galaxy`, `Star`, and `Planet` instances can come from thread-local memory pools.
- Direct library users must pair `Galaxy::initThread`, `Star::initThread`, and `Planet::initThread` with the corresponding release calls on the same thread.

## Verification

For a generator change, perform all applicable checks:

1. Build Debug with viewer disabled.
2. Run fixed-seed parity fixtures in one thread.
3. Compare integer fields exactly and floating fields by IEEE-754 bits when exact parity is required.
4. Build Release and verify the same fixtures.
5. Compare GCC and Clang when changing arithmetic or optimization flags.
6. Benchmark only after correctness checks pass.

Performance changes must preserve generated results. Capture the compiler, build type, thread count, seed interval, star count, plugin set, Prototype snapshot, elapsed time, and processed seeds per second. Compare multiple runs and report the median.

## Performance Guidance

Profile before optimizing. Likely hot areas are star-position collision checks, repeated transcendental functions in star and planet generation, allocation/reset behavior in the object pools, and plugin dispatch. Do not cache, vectorize, reorder, or approximate parity-sensitive arithmetic without exact fixed-seed comparisons.

Prefer optimizations that remove work the selected scan mode provably does not need. Preserve all random-number consumption needed by later retained outputs.
