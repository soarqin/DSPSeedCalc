# 算法 14 精确矿量支持设计

## 结论

`PlanetAlgorithm14` 只覆盖 `GenerateTerrain` 和 `GenerateVegetables`，没有覆盖 `GenerateVeins`。游戏通过 `PlanetModelingManager.Algorithm` 创建 `PlanetAlgorithm14` 后，矿物生成继续调用基类 `PlanetAlgorithm.GenerateVeins`。

当前 C++ 实现把算法 14 标记为不支持，是首版范围限制，不是参考游戏的行为。修正范围仅包括算法 14 地形及支持范围判断；矿物生成继续使用现有通用实现。

## 实现

- 在 `dspugen/terrain/planet_algorithm.cc` 中按参考顺序移植 `PlanetAlgorithm14.GenerateTerrain`。
- 保留 4 次 `DotNet35Random::next()`、4 个 `SimplexNoise` 实例及未使用结果的噪声调用。
- 保留 `System.Math` 双精度运算、`Mathf` 单精度转换点、顺序坐标扰动和最终整数窄化。
- 将 terrain dispatch 的算法 14 结果改为 `Generated`。
- 将精确矿量入口允许的最大算法 ID 从 13 调整为 14。
- 不给算法 14 增加矿脉中心或节点过滤特例；它与游戏一样使用通用规则。

## 验证

- terrain dispatch 测试要求算法 14 返回 `Generated` 且写入非零高度数据。
- 合成行星测试要求 `ensureVeinAmounts()` 对算法 14 返回 `Generated`。
- 算法 15 继续返回 `UnsupportedAlgorithm`，保留显式错误边界。
- 默认 Oracle 必须保持不变；精确矿量 Oracle 仅在固定 fixture 实际包含算法 14 时更新。
