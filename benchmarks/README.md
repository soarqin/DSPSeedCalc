# 性能基准

基准脚本在独立临时目录中运行，每个目录只包含匹配的 `Prototypes` 和一个合成插件。默认执行 1 次预热和 7 次正式采样，保留原始样本，并报告 wall time 的中位数和 MAD。

```powershell
cmake -S . -B cmake-build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 `
  -DBUILD_VIEWER=OFF -DFMT_TEST=OFF `
  -DDSP_BUILD_BENCHMARK_PLUGINS=ON
cmake --build cmake-build-release --target DSPSeedCalc `
  bench_reject_galaxy bench_reject_star bench_reject_planet `
  bench_high_output bench_pose_sink
python tools/benchmark.py cmake-build-release/bin/DSPSeedCalc.exe `
  --plugin-dir cmake-build-release/bin/benchmark-plugins --tier smoke
```

完整矩阵定义在 `benchmarks/workloads.json`。基准结果必须同时记录编译器、构建类型、线程数、种子范围、星数、插件、Prototype 哈希、输出字节数和硬件信息。跨版本比较应复用相同 manifest，不得按速度动态改变种子样本。

PGO 训练集位于 `benchmarks/training-workloads.json`，通过 `tools/pgo.py` 运行。GCC 使用 `DSP_PGO=GENERATE|USE` 和 `DSP_PGO_PROFILE_DIR`；Clang 生成阶段额外设置 `LLVM_PROFILE_FILE`，使用阶段通过 `DSP_PGO_PROFILE_DATA` 指向 `llvm-profdata merge` 生成的文件。PGO、LTO 和 `DSP_ENABLE_NATIVE_ARCH` 相互独立，均不改变默认发布配置。
