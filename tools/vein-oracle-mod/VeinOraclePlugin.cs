// Reference dump for the precise vein amount port.
//
// This runs inside the shipped game process, so every value it writes comes
// from the game's own managed code executing on the game's own Mono runtime,
// including the three Unity math routines that live in engine native code and
// cannot be recovered from the managed assemblies (Vector3.Slerp,
// Quaternion.FromToRotation, Quaternion.AngleAxis).
//
// It needs no savegame: it builds galaxies straight from UniverseGen and drives
// the same generation sequence PlanetModelingManager uses on its worker thread.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.Serialization;
using System.Text;
using BepInEx;
using BepInEx.Configuration;
using UnityEngine;

namespace DspVeinOracle {

[BepInPlugin("dspseedcalc.veinoracle", "DSPSeedCalc Vein Oracle", "1.0.0")]
public class VeinOraclePlugin : BaseUnityPlugin {
    private const int VeinTypeCount = 15;
    private const string NL = "\n";

    private ConfigEntry<string> seedList;
    private ConfigEntry<int> starCount;
    private ConfigEntry<string> outputDirectory;
    private ConfigEntry<bool> quitWhenDone;
    private ConfigEntry<string> heightPlanets;
    private ConfigEntry<bool> dumpVertices;
    private ConfigEntry<string> mode;
    private ConfigEntry<int> threads;
    private ConfigEntry<string> nodePlanets;
    private HashSet<int> nodePlanetIds;
    private StringBuilder nodeText;
    private HashSet<int> heightPlanetIds;
    private string outputRoot;

    private bool finished;
    private int settleFrames;

    private void Awake() {
        seedList = Config.Bind("Oracle", "Seeds", "0,1",
            "Comma separated galaxy seeds to dump.");
        starCount = Config.Bind("Oracle", "StarCount", 64,
            "Star count used for every seed.");
        outputDirectory = Config.Bind("Oracle", "OutputDirectory", "",
            "Directory for the dump files. Empty means the game folder.");
        quitWhenDone = Config.Bind("Oracle", "QuitWhenDone", true,
            "Quit the game once the dump is written.");
        heightPlanets = Config.Bind("Oracle", "HeightPlanets", "",
            "Comma separated planet ids whose raw heightData is written out.");
        dumpVertices = Config.Bind("Oracle", "DumpVertices", true,
            "Write the precision-200 vertex grid produced by Vector3.Slerp.");
        mode = Config.Bind("Oracle", "Mode", "veins",
            "veins = terrain and vein amounts; meta = star and planet fields only.");
        threads = Config.Bind("Oracle", "Threads", 1,
            "Worker threads for the vein pass. 1 keeps the run strictly serial.");
        nodePlanets = Config.Bind("Oracle", "NodePlanets", "",
            "Planet ids whose individual vein nodes are written out, for "
            + "attributing an amount difference to specific nodes.");
        Logger.LogInfo("Vein oracle loaded; waiting for LDB and vein prototypes.");
    }

    private void Update() {
        if (finished) return;
        if (LDB.themes == null || LDB.themes.dataArray == null) return;
        if (LDB.veins == null || LDB.veins.dataArray == null) return;
        if (PlanetModelingManager.veinProtos == null) return;
        // Give the menu a few frames so nothing is still being registered.
        if (++settleFrames < 120) return;

        finished = true;
        try {
            Run();
        } catch (Exception error) {
            Logger.LogError("Vein oracle failed: " + error);
        }
        if (quitWhenDone.Value) {
            Logger.LogInfo("Vein oracle done; quitting.");
            Application.Quit();
        }
    }

    private string ResolveOutputDirectory() {
        var configured = outputDirectory.Value;
        if (!string.IsNullOrEmpty(configured)) {
            Directory.CreateDirectory(configured);
            return configured;
        }
        return Path.GetDirectoryName(Application.dataPath);
    }

    private void Run() {
        var directory = ResolveOutputDirectory();
        outputRoot = directory;
        nodePlanetIds = new HashSet<int>();
        foreach (var part in nodePlanets.Value.Split(',')) {
            var trimmed = part.Trim();
            if (trimmed.Length > 0) nodePlanetIds.Add(int.Parse(trimmed, CultureInfo.InvariantCulture));
        }
        nodeText = new StringBuilder();
        nodeText.Append("# dsp-vein-nodes v1\n");
        heightPlanetIds = new HashSet<int>();
        foreach (var part in heightPlanets.Value.Split(',')) {
            var trimmed = part.Trim();
            if (trimmed.Length > 0) heightPlanetIds.Add(int.Parse(trimmed, CultureInfo.InvariantCulture));
        }
        var protoPath = Path.Combine(directory, "vein-parity-game-protos.txt");
        File.WriteAllText(protoPath, BuildProtoFingerprint(), new UTF8Encoding(false));
        Logger.LogInfo("Wrote " + protoPath);

        if (dumpVertices.Value) {
            // CalcVerts depends only on the precision. The grid comes out of
            // Vector3.Slerp, which is engine native code and therefore the one
            // input to terrain generation that cannot be read off the managed
            // assemblies.
            var grid = new PlanetRawData(200);
            grid.CalcVerts();
            // Buffer.BlockCopy only accepts primitive arrays, so the struct
            // array is written component by component.
            var path = Path.Combine(directory, "verts-200-game.bin");
            using (var stream = new FileStream(path, FileMode.Create, FileAccess.Write))
            using (var writer = new BinaryWriter(stream)) {
                for (var i = 0; i < grid.vertices.Length; i++) {
                    writer.Write(grid.vertices[i].x);
                    writer.Write(grid.vertices[i].y);
                    writer.Write(grid.vertices[i].z);
                }
            }
            Logger.LogInfo("Wrote " + path);
        }

        if (mode.Value == "icall") {
            var icallPath = Path.Combine(directory, "icall-addresses.txt");
            File.WriteAllText(icallPath, BuildIcallDump(), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + icallPath);
            return;
        }

        if (mode.Value == "algo3") {
            var tracePath = Path.Combine(directory, "algo3-trace-game.txt");
            File.WriteAllText(tracePath, BuildAlgo3Trace(), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + tracePath);
            return;
        }

        if (mode.Value == "noise") {
            var noisePath = Path.Combine(directory, "noise-game.txt");
            File.WriteAllText(noisePath, BuildNoiseDump(), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + noisePath);
            return;
        }

        if (mode.Value == "libm") {
            var libmPath = Path.Combine(directory, "libm-game.txt");
            File.WriteAllText(libmPath, BuildLibmDump(), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + libmPath);
            return;
        }

        var seeds = ParseSeeds(seedList.Value);
        if (mode.Value == "meta") {
            var metaPath = Path.Combine(directory, "meta-parity-game.txt");
            File.WriteAllText(metaPath, BuildMetaDump(seeds), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + metaPath);
            return;
        }
        var dumpPath = Path.Combine(directory, "vein-parity-game.txt");
        File.WriteAllText(dumpPath, BuildDump(seeds, dumpPath), new UTF8Encoding(false));
        Logger.LogInfo("Wrote " + dumpPath);
        if (nodePlanetIds.Count > 0) {
            var nodePath = Path.Combine(directory, "vein-nodes-game.txt");
            File.WriteAllText(nodePath, nodeText.ToString(), new UTF8Encoding(false));
            Logger.LogInfo("Wrote " + nodePath);
        }
    }

    private static List<int> ParseSeeds(string value) {
        var seeds = new List<int>();
        foreach (var part in value.Split(',')) {
            var trimmed = part.Trim();
            if (trimmed.Length == 0) continue;
            var dash = trimmed.IndexOf('-', 1);
            if (dash > 0) {
                var from = int.Parse(trimmed.Substring(0, dash), CultureInfo.InvariantCulture);
                var to = int.Parse(trimmed.Substring(dash + 1), CultureInfo.InvariantCulture);
                for (var seed = from; seed <= to; seed++) seeds.Add(seed);
            } else {
                seeds.Add(int.Parse(trimmed, CultureInfo.InvariantCulture));
            }
        }
        return seeds;
    }

    // Records exactly the prototype fields the vein path consumes, so the dump
    // can be rejected if another mod changed LDB, and so the repository's
    // Prototypes snapshot can be checked against the running game.
    private string BuildProtoFingerprint() {
        var text = new StringBuilder();
        text.Append("# dsp-vein-parity-protos v1\n");
        text.Append("# gameVersion=").Append(GameConfig.gameVersion.ToFullString()).Append('\n');
        text.Append("themeCount=").Append(LDB.themes.dataArray.Length).Append('\n');
        foreach (var theme in LDB.themes.dataArray) {
            text.Append("theme id=").Append(theme.ID);
            text.Append(" distribute=").Append((int)theme.Distribute);
            text.Append(" planetType=").Append((int)theme.PlanetType);
            text.Append(" waterItemId=").Append(theme.WaterItemId);
            AppendFloat(text, " waterHeight=", theme.WaterHeight);
            AppendFloat(text, " modX0=", theme.ModX.x);
            AppendFloat(text, " modX1=", theme.ModX.y);
            AppendFloat(text, " modY0=", theme.ModY.x);
            AppendFloat(text, " modY1=", theme.ModY.y);
            AppendInts(text, " algos=", theme.Algos);
            AppendInts(text, " veinSpot=", theme.VeinSpot);
            AppendFloats(text, " veinCount=", theme.VeinCount);
            AppendFloats(text, " veinOpacity=", theme.VeinOpacity);
            AppendInts(text, " rareVeins=", theme.RareVeins);
            AppendFloats(text, " rareSettings=", theme.RareSettings);
            text.Append('\n');
        }
        text.Append("veinProtoCount=").Append(LDB.veins.dataArray.Length).Append('\n');
        foreach (var vein in LDB.veins.dataArray) {
            text.Append("vein id=").Append(vein.ID);
            text.Append(" modelIndex=").Append(vein.ModelIndex);
            text.Append(" modelCount=").Append(vein.ModelCount);
            text.Append(" miningItem=").Append(vein.MiningItem);
            text.Append('\n');
        }
        return text.ToString();
    }

    private static void AppendFloat(StringBuilder text, string label, float value) {
        text.Append(label).Append(value.ToString("R", CultureInfo.InvariantCulture));
    }

    private static void AppendInts(StringBuilder text, string label, int[] values) {
        text.Append(label);
        if (values == null) { text.Append('-'); return; }
        for (var i = 0; i < values.Length; i++) {
            if (i > 0) text.Append(';');
            text.Append(values[i].ToString(CultureInfo.InvariantCulture));
        }
        if (values.Length == 0) text.Append('-');
    }

    private static void AppendFloats(StringBuilder text, string label, float[] values) {
        text.Append(label);
        if (values == null) { text.Append('-'); return; }
        for (var i = 0; i < values.Length; i++) {
            if (i > 0) text.Append(';');
            text.Append(values[i].ToString("R", CultureInfo.InvariantCulture));
        }
        if (values.Length == 0) text.Append('-');
    }

    private string BuildDump(List<int> seeds, string dumpPath) {
        var text = new StringBuilder();
        text.Append("# dsp-vein-parity v1\n");
        text.Append("# gameVersion=").Append(GameConfig.gameVersion.ToFullString()).Append('\n');
        text.Append("# resourceMultiplier=1 isRareResource=0 isInfiniteResource=0\n");

        var previousGameDesc = DSPGame.GameDesc;
        var previousData = GameMain.data;
        try {
            foreach (var seed in seeds) {
                var description = BuildGameDesc(seed, starCount.Value);
                DSPGame.GameDesc = description;
                // GenerateVeins reads GameMain.data.gameDesc for the resource
                // switches. At the menu that object does not exist yet, so a
                // bare instance carries the description without running any of
                // GameData's initialisation.
                GameMain.data = (GameData)FormatterServices.GetUninitializedObject(typeof(GameData));
                GameMain.data.gameDesc = description;
                AppendGalaxy(text, description);
                // A 256 seed run takes hours; flush after every galaxy so a
                // crash near the end leaves a usable prefix instead of nothing.
                if (dumpPath != null)
                    File.WriteAllText(dumpPath, text.ToString(), new UTF8Encoding(false));
            }
        } finally {
            DSPGame.GameDesc = previousGameDesc;
            GameMain.data = previousData;
        }
        return text.ToString();
    }

    // Intermediates of PlanetAlgorithm3 for named cells, to attribute a height
    // difference to a specific step rather than to the algorithm as a whole.
    private string BuildAlgo3Trace() {
        var text = new StringBuilder();
        text.Append("# dsp-algo3-trace v1\n");
        var description = BuildGameDesc(0, 64);
        var previousGameDesc = DSPGame.GameDesc;
        var previousData = GameMain.data;
        DSPGame.GameDesc = description;
        GameMain.data = (GameData)FormatterServices.GetUninitializedObject(typeof(GameData));
        GameMain.data.gameDesc = description;
        try {
            var galaxy = UniverseGen.CreateGalaxy(description);
            PlanetData planet = null;
            for (var si = 0; si < galaxy.starCount && planet == null; si++)
                for (var pi = 0; pi < galaxy.stars[si].planetCount; pi++)
                    if (galaxy.stars[si].planets[pi].id == 103) {
                        planet = galaxy.stars[si].planets[pi];
                        break;
                    }
            if (planet == null) return text.Append("planet 103 not found\n").ToString();
            planet.data = new PlanetRawData(planet.precision);
            planet.data.CalcVerts();
            text.Append("modX=").Append(DoubleBits(planet.mod_x))
                .Append(" radius=").Append(FloatBits(planet.radius))
                .Append(" seed=").Append(planet.seed).Append('\n');

            double num = 0.007, num2 = 0.007, num3 = 0.007;
            var rng = new DotNet35Random(planet.seed);
            var noise1 = new SimplexNoise(rng.Next());
            var noise2 = new SimplexNoise(rng.Next());
            var modX = planet.mod_x;
            int[] cells = { 270, 274, 367, 368, 738, 768, 1048, 1141, 0, 1, 2 };
            foreach (var i in cells) {
                double x = planet.data.vertices[i].x * planet.radius;
                double y = planet.data.vertices[i].y * planet.radius;
                double z = planet.data.vertices[i].z * planet.radius;
                text.Append("cell ").Append(i);
                text.Append(" vx=").Append(FloatBits(planet.data.vertices[i].x));
                text.Append(" vy=").Append(FloatBits(planet.data.vertices[i].y));
                text.Append(" vz=").Append(FloatBits(planet.data.vertices[i].z));
                text.Append(" x0=").Append(DoubleBits(x));
                x += Math.Sin(y * 0.15) * 3.0;
                y += Math.Sin(z * 0.15) * 3.0;
                z += Math.Sin(x * 0.15) * 3.0;
                text.Append(" x=").Append(DoubleBits(x));
                text.Append(" y=").Append(DoubleBits(y));
                text.Append(" z=").Append(DoubleBits(z));
                double n11 = noise1.Noise3DFBM(x * num * 1.0, y * num2 * 1.1, z * num3 * 1.0, 6, 0.5, 1.8);
                double n12 = noise2.Noise3DFBM(x * num * 1.3 + 0.5, y * num2 * 2.8 + 0.2, z * num3 * 1.3 + 0.7, 3) * 2.0;
                double n13 = noise2.Noise3DFBM(x * num * 6.0, y * num2 * 12.0, z * num3 * 6.0, 2) * 2.0;
                n13 = n13 + (n13 * 0.1 - n13) * modX;
                double n14 = noise2.Noise3DFBM(x * num * 0.8, y * num2 * 0.8, z * num3 * 0.8, 2) * 2.0;
                text.Append(" n11=").Append(DoubleBits(n11));
                text.Append(" n12=").Append(DoubleBits(n12));
                text.Append(" n13=").Append(DoubleBits(n13));
                text.Append(" n14=").Append(DoubleBits(n14));
                double n15 = n11 * 2.0 + 0.92;
                double n16 = n12 * (double)Mathf.Abs((float)n14 + 0.5f);
                n15 += (double)Mathf.Clamp01((float)(n16 - 0.35) * 1f);
                text.Append(" n15=").Append(DoubleBits(n15));
                double n17 = Maths.Levelize2(n15);
                if (n17 > 0.0) n17 = Maths.Levelize4(Maths.Levelize2(n15)) + (Maths.Levelize2(n15) - Maths.Levelize4(Maths.Levelize2(n15))) * modX;
                text.Append(" n17=").Append(DoubleBits(n17));
                text.Append(" h=").Append(planet.data.heightData[i]);
                text.Append('\n');
            }
        } finally {
            DSPGame.GameDesc = previousGameDesc;
            GameMain.data = previousData;
        }
        return text.ToString();
    }

    // Every SimplexNoise entry point the terrain algorithms use, over inputs of
    // the same magnitude they see in practice (vertex coordinates scaled by the
    // planet radius), for several seeds.
    private string BuildNoiseDump() {
        var text = new StringBuilder();
        text.Append("# dsp-noise v1\n");
        text.Append("# gameVersion=").Append(GameConfig.gameVersion.ToFullString()).Append('\n');
        var seeds = new int[] { 12345, 1, 987654321 };
        foreach (var seed in seeds) {
            var noise = new SimplexNoise(seed);
            text.Append("seed ").Append(seed).Append('\n');
            for (var i = 0; i < 400; i++) {
                // Roughly the coordinate range a precision-200 planet produces.
                var x = i * 0.37 - 74.0;
                var y = i * -0.53 + 41.0;
                var z = i * 0.191 - 19.0;
                text.Append("n1 ").Append(DoubleBits(noise.Noise(x))).Append('\n');
                text.Append("n2 ").Append(DoubleBits(noise.Noise(x, y))).Append('\n');
                text.Append("n3 ").Append(DoubleBits(noise.Noise(x, y, z))).Append('\n');
                text.Append("n4 ").Append(DoubleBits(noise.Noise(x, y, z, 0.37))).Append('\n');
                text.Append("f2 ").Append(DoubleBits(noise.Noise2DFBM(x, y, 4))).Append('\n');
                for (var oct = 1; oct <= 6; oct++)
                    text.Append("f3o").Append(oct).Append(' ')
                        .Append(DoubleBits(noise.Noise3DFBM(x * 0.01, y * 0.012, z * 0.01, oct)))
                        .Append('\n');
                text.Append("f3p ").Append(DoubleBits(
                    noise.Noise3DFBM(x * 0.007, y * 0.007, z * 0.007, 6, 0.45, 1.8))).Append('\n');
                text.Append("f3a ").Append(DoubleBits(
                    noise.Noise3DFBMInitialAmp(x * 0.01, y * 0.01, z * 0.01, 6, 0.6, 2.0, 0.7))).Append('\n');
                text.Append("rid ").Append(DoubleBits(
                    noise.RidgedNoise(x * 0.01, y * 0.01, z * 0.01, 6, 0.7, 2.0, 0.8))).Append('\n');
            }
        }
        return text.ToString();
    }

    // Raw System.Math results for the transcendentals the generator uses, so a
    // divergence can be attributed to the C runtime rather than to the port.
    private string BuildLibmDump() {
        var text = new StringBuilder();
        text.Append("# dsp-libm v1\n");
        text.Append("# gameVersion=").Append(GameConfig.gameVersion.ToFullString()).Append('\n');
        // Ranges chosen to match how the generator actually calls these: the
        // RandNormal composite sweeps sin over a full turn, Mathf.Log feeds the
        // repeated log chain, and Math.Pow is used with several fixed bases and
        // several fixed exponents.
        var exponents = new double[] { 0.25, 0.27, 0.3, 0.33000001311302185, 0.5,
                                       0.56, 0.7, 0.73, 0.8, 1.3, 2.0 };
        var bases = new double[] { 0.1, 1.2, 2.0, 7.0, 10.0 };
        const int steps = 4000;
        for (var i = 0; i <= steps; i++) {
            var u = (double)i / steps;                 // 0 .. 1
            var wide = u * 200.0;                      // 0 .. 200
            var turn = u * Math.PI * 2.0;              // full turn for RandNormal
            foreach (var b in bases)
                text.Append("pow ").Append(DoubleBits(b)).Append(' ')
                    .Append(DoubleBits(u * 8.0 - 4.0)).Append(' ')
                    .Append(DoubleBits(Math.Pow(b, u * 8.0 - 4.0))).Append('\n');
            foreach (var e in exponents)
                text.Append("pow ").Append(DoubleBits(wide)).Append(' ')
                    .Append(DoubleBits(e)).Append(' ')
                    .Append(DoubleBits(Math.Pow(wide, e))).Append('\n');
            text.Append("log ").Append(DoubleBits(wide)).Append(' ')
                .Append(DoubleBits(Math.Log(wide))).Append('\n');
            text.Append("log ").Append(DoubleBits(1.0 - u)).Append(' ')
                .Append(DoubleBits(Math.Log(1.0 - u))).Append('\n');
            text.Append("log10 ").Append(DoubleBits(wide)).Append(' ')
                .Append(DoubleBits(Math.Log10(wide))).Append('\n');
            text.Append("exp ").Append(DoubleBits(u * 20.0 - 10.0)).Append(' ')
                .Append(DoubleBits(Math.Exp(u * 20.0 - 10.0))).Append('\n');
            text.Append("sin ").Append(DoubleBits(turn)).Append(' ')
                .Append(DoubleBits(Math.Sin(turn))).Append('\n');
            text.Append("cos ").Append(DoubleBits(turn)).Append(' ')
                .Append(DoubleBits(Math.Cos(turn))).Append('\n');
            // Algorithms 3, 10 and 14 perturb coordinates with Math.Sin(coord *
            // 0.15) where coord reaches the planet radius, so the argument runs
            // out to about +-30, where argument reduction starts to matter.
            var far = u * 60.0 - 30.0;
            text.Append("sinfar ").Append(DoubleBits(far)).Append(' ')
                .Append(DoubleBits(Math.Sin(far))).Append('\n');
            text.Append("cosfar ").Append(DoubleBits(far)).Append(' ')
                .Append(DoubleBits(Math.Cos(far))).Append('\n');
            text.Append("sqrt ").Append(DoubleBits(wide)).Append(' ')
                .Append(DoubleBits(Math.Sqrt(wide))).Append('\n');
            text.Append("asin ").Append(DoubleBits(u * 2.0 - 1.0)).Append(' ')
                .Append(DoubleBits(Math.Asin(u * 2.0 - 1.0))).Append('\n');
            text.Append("atan2 ").Append(DoubleBits(u * 2.0 - 1.0)).Append(' ')
                .Append(DoubleBits(1.0 - u)).Append(' ')
                .Append(DoubleBits(Math.Atan2(u * 2.0 - 1.0, 1.0 - u))).Append('\n');
        }
        // The exact RandNormal composite, which is where star mass comes from.
        for (var i = 0; i <= 600; i++) {
            for (var j = 0; j <= 600; j += 7) {
                var r1 = (double)i / 601.0;
                var r2 = (double)j / 601.0;
                var value = Math.Sqrt(-2.0 * Math.Log(1.0 - r1)) *
                            Math.Sin(Math.PI * 2.0 * r2);
                text.Append("randnormal ").Append(DoubleBits(r1)).Append(' ')
                    .Append(DoubleBits(r2)).Append(' ')
                    .Append(DoubleBits(value)).Append('\n');
            }
        }
        return text.ToString();
    }

    // Star and planet fields only: no terrain, no veins. Cheap enough to cover
    // hundreds of seeds, which is what catches rare floating-point divergence
    // in the parts of the generator that are not vein specific.
    private string BuildMetaDump(List<int> seeds) {
        var text = new StringBuilder();
        text.Append("# dsp-meta-parity v1\n");
        text.Append("# gameVersion=").Append(GameConfig.gameVersion.ToFullString()).Append('\n');

        var previousGameDesc = DSPGame.GameDesc;
        var previousData = GameMain.data;
        try {
            foreach (var seed in seeds) {
                var description = BuildGameDesc(seed, starCount.Value);
                DSPGame.GameDesc = description;
                GameMain.data = (GameData)FormatterServices.GetUninitializedObject(typeof(GameData));
                GameMain.data.gameDesc = description;
                var galaxy = UniverseGen.CreateGalaxy(description);
                text.Append("galaxy seed=").Append(galaxy.seed);
                text.Append(" stars=").Append(galaxy.starCount);
                text.Append(" birthStar=").Append(galaxy.birthStarId);
                text.Append(" birthPlanet=").Append(galaxy.birthPlanetId);
                text.Append(" habitable=").Append(galaxy.habitableCount);
                text.Append('\n');
                for (var i = 0; i < galaxy.starCount; i++) AppendStar(text, galaxy.stars[i]);
                if (seeds.Count > 16 && (seed & 31) == 0) Logger.LogInfo("meta seed " + seed);
            }
        } finally {
            DSPGame.GameDesc = previousGameDesc;
            GameMain.data = previousData;
        }
        return text.ToString();
    }

    private static void AppendStar(StringBuilder text, StarData star) {
        if (star == null) return;
        text.Append("star id=").Append(star.id);
        text.Append(" index=").Append(star.index);
        text.Append(" seed=").Append(star.seed);
        text.Append(" type=").Append((int)star.type);
        text.Append(" spectr=").Append((int)star.spectr);
        text.Append(" name=").Append(star.name);
        text.Append(" posX=").Append(DoubleBits(star.position.x));
        text.Append(" posY=").Append(DoubleBits(star.position.y));
        text.Append(" posZ=").Append(DoubleBits(star.position.z));
        text.Append(" mass=").Append(FloatBits(star.mass));
        text.Append(" lifetime=").Append(FloatBits(star.lifetime));
        text.Append(" age=").Append(FloatBits(star.age));
        text.Append(" temperature=").Append(FloatBits(star.temperature));
        text.Append(" luminosity=").Append(FloatBits(star.luminosity));
        text.Append(" radius=").Append(FloatBits(star.radius));
        text.Append(" habitableRadius=").Append(FloatBits(star.habitableRadius));
        text.Append(" lightBalanceRadius=").Append(FloatBits(star.lightBalanceRadius));
        text.Append(" orbitScaler=").Append(FloatBits(star.orbitScaler));
        text.Append(" dysonRadius=").Append(FloatBits(star.dysonRadius));
        text.Append(" color=").Append(FloatBits(star.color));
        text.Append(" classFactor=").Append(FloatBits(star.classFactor));
        text.Append(" resourceCoef=").Append(FloatBits(star.resourceCoef));
        text.Append(" planets=").Append(star.planetCount);
        text.Append('\n');
        for (var i = 0; i < star.planetCount; i++) AppendPlanet(text, star.planets[i]);
    }

    private static void AppendPlanet(StringBuilder text, PlanetData planet) {
        if (planet == null) return;
        text.Append("planet id=").Append(planet.id);
        text.Append(" index=").Append(planet.index);
        text.Append(" seed=").Append(planet.seed);
        text.Append(" number=").Append(planet.number);
        text.Append(" type=").Append((int)planet.type);
        text.Append(" theme=").Append(planet.theme);
        text.Append(" algo=").Append(planet.algoId);
        text.Append(" singularity=").Append((int)planet.singularity);
        text.Append(" orbitAround=").Append(planet.orbitAround);
        text.Append(" orbitIndex=").Append(planet.orbitIndex);
        text.Append(" orbitRadius=").Append(FloatBits(planet.orbitRadius));
        text.Append(" radius=").Append(FloatBits(planet.radius));
        text.Append(" scale=").Append(FloatBits(planet.scale));
        text.Append(" habitableBias=").Append(FloatBits(planet.habitableBias));
        text.Append(" sunDistance=").Append(FloatBits(planet.sunDistance));
        text.Append(" temperatureBias=").Append(FloatBits(planet.temperatureBias));
        text.Append(" luminosity=").Append(FloatBits(planet.luminosity));
        text.Append(" orbitInclination=").Append(FloatBits(planet.orbitInclination));
        text.Append(" orbitLongitude=").Append(FloatBits(planet.orbitLongitude));
        text.Append(" orbitalPeriod=").Append(DoubleBits(planet.orbitalPeriod));
        text.Append(" orbitPhase=").Append(FloatBits(planet.orbitPhase));
        text.Append(" obliquity=").Append(FloatBits(planet.obliquity));
        text.Append(" rotationPeriod=").Append(DoubleBits(planet.rotationPeriod));
        text.Append(" rotationPhase=").Append(FloatBits(planet.rotationPhase));
        text.Append(" modX=").Append(DoubleBits(planet.mod_x));
        text.Append(" modY=").Append(DoubleBits(planet.mod_y));
        text.Append(" waterItemId=").Append(planet.waterItemId);
        text.Append(" waterHeight=").Append(FloatBits(planet.waterHeight));
        text.Append(" gas=");
        if (planet.gasItems == null || planet.gasItems.Length == 0) {
            text.Append('-');
        } else {
            for (var i = 0; i < planet.gasItems.Length; i++) {
                if (i > 0) text.Append(';');
                text.Append(planet.gasItems[i]).Append(':')
                    .Append(FloatBits(planet.gasSpeeds[i]));
            }
        }
        text.Append('\n');
    }

    private static GameDesc BuildGameDesc(int seed, int stars) {
        var description = new GameDesc();
        description.SetForNewGame(UniverseGen.algoVersion, seed, stars, 1, 1f);
        return description;
    }

    private void AppendGalaxy(StringBuilder text, GameDesc description) {
        var galaxy = UniverseGen.CreateGalaxy(description);
        text.Append("galaxy seed=").Append(galaxy.seed);
        text.Append(" stars=").Append(galaxy.starCount);
        text.Append(" birthPlanet=").Append(galaxy.birthPlanetId);
        text.Append('\n');
        // resourceCoef scales every vein amount and is produced by Mathf.Log /
        // Mathf.Pow, so it is reported as raw bits to catch last-place drift.
        for (var starIndex = 0; starIndex < galaxy.starCount; starIndex++) {
            var star = galaxy.stars[starIndex];
            if (star == null) continue;
            text.Append("star id=").Append(star.id);
            text.Append(" type=").Append((int)star.type);
            text.Append(" spectr=").Append((int)star.spectr);
            text.Append(" resourceCoef=").Append(FloatBits(star.resourceCoef));
            text.Append('\n');
        }

        var nodeCount = new long[VeinTypeCount];
        var amount = new long[VeinTypeCount];
        for (var starIndex = 0; starIndex < galaxy.starCount; starIndex++) {
            var star = galaxy.stars[starIndex];
            if (star == null) continue;
            for (var planetIndex = 0; planetIndex < star.planetCount; planetIndex++) {
                var planet = star.planets[planetIndex];
                if (planet == null) continue;
                Array.Clear(nodeCount, 0, VeinTypeCount);
                Array.Clear(amount, 0, VeinTypeCount);
                GeneratePlanet(planet, nodeCount, amount);
                if (nodePlanetIds.Contains(planet.id) && planet.data != null) {
                    var pool = planet.data.veinPool;
                    for (var v = 1; v < planet.data.veinCursor; v++) {
                        nodeText.Append("node seed=").Append(galaxy.seed);
                        nodeText.Append(" planet=").Append(planet.id);
                        nodeText.Append(" i=").Append(v);
                        nodeText.Append(" type=").Append((int)pool[v].type);
                        nodeText.Append(" group=").Append(pool[v].groupIndex);
                        nodeText.Append(" model=").Append(pool[v].modelIndex);
                        nodeText.Append(" amount=").Append(pool[v].amount);
                        nodeText.Append(" posX=").Append(FloatBits(pool[v].pos.x));
                        nodeText.Append(" posY=").Append(FloatBits(pool[v].pos.y));
                        nodeText.Append(" posZ=").Append(FloatBits(pool[v].pos.z));
                        nodeText.Append('\n');
                    }
                }
                if (heightPlanetIds != null && heightPlanetIds.Contains(planet.id) && planet.data != null) {
                    var raw = new byte[planet.data.heightData.Length * 2];
                    Buffer.BlockCopy(planet.data.heightData, 0, raw, 0, raw.Length);
                    File.WriteAllBytes(Path.Combine(outputRoot,
                        "height-" + galaxy.seed + "-" + planet.id + ".bin"), raw);
                    // biomoData is already byte[]; dumped alongside height so a
                    // digest mismatch can be localised to individual vertices.
                    File.WriteAllBytes(Path.Combine(outputRoot,
                        "biomo-" + galaxy.seed + "-" + planet.id + ".bin"),
                        planet.data.biomoData);
                }
                var heightDigest = planet.data == null ? "-" : DigestUShort(planet.data.heightData);
                var biomoDigest = planet.data == null ? "-" : DigestByte(planet.data.biomoData);
                text.Append("planet id=").Append(planet.id);
                text.Append(" algo=").Append(planet.algoId);
                text.Append(" theme=").Append(planet.theme);
                text.Append(" modX=").Append(DoubleBits(planet.mod_x));
                text.Append(" modY=").Append(DoubleBits(planet.mod_y));
                text.Append(" radius=").Append(FloatBits(planet.radius));
                text.Append(" height=").Append(heightDigest);
                text.Append(" biomo=").Append(biomoDigest);
                if (planet.id == galaxy.birthPlanetId) {
                    // GenBirthPoints drives the birth planet's whole vein
                    // layout, so its three points are dumped to tell a bad
                    // birth point apart from bad vein placement.
                    AppendVector(text, " birthPt=", planet.birthPoint);
                    AppendVector(text, " birthRes0=", planet.birthResourcePoint0);
                    AppendVector(text, " birthRes1=", planet.birthResourcePoint1);
                    // Re-run GenBirthPoints' own opening steps with the game's
                    // code so the port can be compared stage by stage rather
                    // than only on the final points.
                    var pose = planet.PredictPose(85.0);
                    AppendVector(text, " posePos=", pose.position);
                    text.Append(" poseRot=").Append(FloatBits(pose.rotation.x)).Append(',')
                        .Append(FloatBits(pose.rotation.y)).Append(',')
                        .Append(FloatBits(pose.rotation.z)).Append(',')
                        .Append(FloatBits(pose.rotation.w));
                    Vector3 sd = Maths.QInvRotateLF(pose.rotation,
                        planet.star.uPosition - (VectorLF3)pose.position * 40000.0);
                    AppendVector(text, " starDirRaw=", sd);
                    sd.Normalize();
                    AppendVector(text, " starDir=", sd);
                    var ax0 = Vector3.Cross(sd, Vector3.up).normalized;
                    var ax1 = Vector3.Cross(ax0, sd).normalized;
                    AppendVector(text, " axis0=", ax0);
                    AppendVector(text, " axis1=", ax1);
                    // Quaternion.AngleAxis is a native FreeFunction, so each
                    // stage of the rotation chain is dumped separately to see
                    // whether it, or the managed multiply, is the divergence.
                    var qLon = Quaternion.AngleAxis(planet.orbitLongitude, Vector3.up);
                    var qInc = Quaternion.AngleAxis(planet.orbitInclination, Vector3.forward);
                    var qObl = Quaternion.AngleAxis(planet.obliquity, Vector3.forward);
                    AppendQuat(text, " qLon=", qLon);
                    AppendQuat(text, " qInc=", qInc);
                    AppendQuat(text, " qObl=", qObl);
                    AppendQuat(text, " qOrbit=", qLon * qInc);
                    AppendQuat(text, " qSystem=", qLon * qInc * qObl);
                    text.Append(" orbitLon=").Append(FloatBits(planet.orbitLongitude));
                    text.Append(" orbitInc=").Append(FloatBits(planet.orbitInclination));
                    text.Append(" obliq=").Append(FloatBits(planet.obliquity));
                }
                AppendLongs(text, " nodes=", nodeCount);
                AppendLongs(text, " amounts=", amount);
                text.Append('\n');
                // The raw data for one precision-200 planet is a few megabytes;
                // drop it before moving on so a 64 star galaxy does not hold
                // every planet's terrain at once.
                planet.data = null;
                planet.modData = null;
                planet.aux = null;
            }
        }
        Logger.LogInfo("Dumped galaxy seed " + galaxy.seed);
    }

    // Mirrors PlanetModelingManager's generation sequence exactly, including the
    // vegetable pass: GenerateVeins erases vegetables under each node, so the
    // pass is kept rather than assumed irrelevant.
    private static void GeneratePlanet(PlanetData planet, long[] nodeCount, long[] amount) {
        var algorithm = PlanetModelingManager.Algorithm(planet);
        if (algorithm == null) return;
        planet.data = new PlanetRawData(planet.precision);
        planet.modData = planet.data.InitModData(planet.modData);
        planet.data.CalcVerts();
        planet.aux = new PlanetAuxData(planet);
        algorithm.GenerateTerrain(planet.mod_x, planet.mod_y);
        algorithm.CalcWaterPercent();
        planet.data.vegeCursor = 1;
        if (planet.type != EPlanetType.Gas) {
            algorithm.GenerateVegetables();
        }
        planet.data.veinCursor = 1;
        if (planet.type != EPlanetType.Gas) {
            algorithm.GenerateVeins();
        }

        var pool = planet.data.veinPool;
        for (var i = 1; i < planet.data.veinCursor; i++) {
            var type = (int)pool[i].type;
            if (type < 0 || type >= VeinTypeCount) continue;
            nodeCount[type]++;
            amount[type] += pool[i].amount;
        }
    }

    private static string FloatBits(float value) {
        var bits = BitConverter.ToUInt32(BitConverter.GetBytes(value), 0);
        return bits.ToString("X8", CultureInfo.InvariantCulture);
    }

    private static string DoubleBits(double value) {
        return ((ulong)BitConverter.DoubleToInt64Bits(value)).ToString(
            "X16", CultureInfo.InvariantCulture);
    }

    // FNV-1a over the raw buffer: enough to tell "terrain differs" from
    // "terrain matches but vein placement differs" without shipping megabytes.
    private static string DigestUShort(ushort[] values) {
        ulong hash = 14695981039346656037UL;
        for (var i = 0; i < values.Length; i++) {
            hash = (hash ^ (byte)values[i]) * 1099511628211UL;
            hash = (hash ^ (byte)(values[i] >> 8)) * 1099511628211UL;
        }
        return hash.ToString("X16", CultureInfo.InvariantCulture);
    }

    private static string DigestByte(byte[] values) {
        ulong hash = 14695981039346656037UL;
        for (var i = 0; i < values.Length; i++) {
            hash = (hash ^ values[i]) * 1099511628211UL;
        }
        return hash.ToString("X16", CultureInfo.InvariantCulture);
    }


    [System.Runtime.InteropServices.DllImport("kernel32", CharSet = System.Runtime.InteropServices.CharSet.Ansi)]
    private static extern IntPtr GetModuleHandleA(string name);

    [System.Runtime.InteropServices.DllImport("mono-2.0-bdwgc")]
    private static extern IntPtr mono_lookup_internal_call(IntPtr method);

    // Mono stores the MonoMethod* directly in RuntimeMethodHandle.Value, and
    // mono_lookup_internal_call maps it to the native FreeFunction.  Subtracting
    // UnityPlayer.dll's base gives an RVA that can be opened straight in IDA
    // (image base 0x180000000), which beats guessing at the icall tables.
    private static string BuildIcallDump() {
        var text = new StringBuilder();
        text.Append("# dsp-icall-addresses v1").Append(NL);
        var unityBase = GetModuleHandleA("UnityPlayer.dll");
        text.Append("UnityPlayer.dll base=").Append(unityBase.ToInt64().ToString("X")).Append(NL);
        var wanted = new[] {
            new[]{"UnityEngine.Quaternion","AngleAxis_Injected"},
            new[]{"UnityEngine.Quaternion","FromToRotation_Injected"},
            new[]{"UnityEngine.Vector3","Slerp_Injected"},
            new[]{"UnityEngine.Quaternion","Internal_ToAxisAngleRad_Injected"},
        };
        foreach (var w in wanted) {
            try {
                var t = typeof(Vector3).Assembly.GetType(w[0]);
                var mi = t.GetMethod(w[1], BindingFlags.NonPublic | BindingFlags.Static);
                if (mi == null) { text.Append(w[0]).Append(".").Append(w[1]).Append(" = NOT FOUND").Append(NL); continue; }
                var fn = mono_lookup_internal_call(mi.MethodHandle.Value);
                long rva = fn.ToInt64() - unityBase.ToInt64();
                text.Append(w[0]).Append(".").Append(w[1])
                    .Append(" addr=").Append(fn.ToInt64().ToString("X"))
                    .Append(" rva=").Append(rva.ToString("X"))
                    .Append(" ida=").Append((0x180000000L + rva).ToString("X")).Append(NL);
            } catch (Exception e) {
                text.Append(w[0]).Append(".").Append(w[1]).Append(" = ERROR ").Append(e.Message).Append(NL);
            }
        }
        return text.ToString();
    }

    private static void AppendQuat(StringBuilder text, string label, Quaternion q) {
        text.Append(label).Append(FloatBits(q.x)).Append(',').Append(FloatBits(q.y))
            .Append(',').Append(FloatBits(q.z)).Append(',').Append(FloatBits(q.w));
    }

    private static void AppendVector(StringBuilder text, string label, Vector3 value) {
        text.Append(label).Append(FloatBits(value.x)).Append(',')
            .Append(FloatBits(value.y)).Append(',').Append(FloatBits(value.z));
    }

    private static void AppendLongs(StringBuilder text, string label, long[] values) {
        text.Append(label);
        for (var i = 0; i < values.Length; i++) {
            text.Append(values[i].ToString(CultureInfo.InvariantCulture)).Append(',');
        }
    }
}

}
