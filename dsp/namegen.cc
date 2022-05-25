/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "namegen.hh"
#include "util/dotnet35random.hh"
#include <fmt/format.h>

void strReplace(std::string &subject, const std::string &search,
                const std::string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}
template<typename T, int N>
char (&dim_helper(T(&)[N]))[N];
#define dim(x) (sizeof(dim_helper(x)))

static const std::string con0[39] =
    {
        "p", "t", "c", "k", "b", "d", "g", "f", "ph", "s",
        "sh", "th", "h", "v", "z", "th", "r", "ch", "tr", "dr",
        "m", "n", "l", "y", "w", "sp", "st", "sk", "sc", "sl",
        "pl", "cl", "bl", "gl", "fr", "fl", "pr", "br", "cr"
    };

static const std::string con1[16] =
    {
        "thr", "ex", "ec", "el", "er", "ev", "il", "is", "it", "ir",
        "up", "ut", "ur", "un", "gt", "phr"
    };

static const std::string vow0[7] = {"a", "an", "am", "al", "o", "u", "xe"};

static const std::string vow1[23] =
    {
        "ea", "ee", "ie", "i", "e", "a", "er", "a", "u", "oo",
        "u", "or", "o", "oa", "ar", "a", "ei", "ai", "i", "au",
        "ou", "ao", "ir"
    };

static const std::string vow2[7] = {"y", "oi", "io", "iur", "ur", "ac", "ic"};

static const std::string ending[18] =
    {
        "er", "n", "un", "or", "ar", "o", "o", "ans", "us", "ix",
        "us", "iurs", "a", "eo", "urn", "es", "eon", "y"
    };

static const std::string roman[21] =
    {
        "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX",
        "X", "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX",
        "XX"
    };

static const std::string constellations[88] =
    {
        "Andromedae", "Antliae", "Apodis", "Aquarii", "Aquilae", "Arae", "Arietis", "Aurigae", "Bootis", "Caeli",
        "Camelopardalis", "Cancri", "Canum Venaticorum", "Canis Majoris", "Canis Minoris", "Capricorni", "Carinae",
        "Cassiopeiae", "Centauri", "Cephei",
        "Ceti", "Chamaeleontis", "Circini", "Columbae", "Comae Berenices", "Coronae Australis", "Coronae Borealis",
        "Corvi", "Crateris", "Crucis",
        "Cygni", "Delphini", "Doradus", "Draconis", "Equulei", "Eridani", "Fornacis", "Geminorum", "Gruis", "Herculis",
        "Horologii", "Hydrae", "Hydri", "Indi", "Lacertae", "Leonis", "Leonis Minoris", "Leporis", "Librae", "Lupi",
        "Lyncis", "Lyrae", "Mensae", "Microscopii", "Monocerotis", "Muscae", "Normae", "Octantis", "Ophiuchii",
        "Orionis",
        "Pavonis", "Pegasi", "Persei", "Phoenicis", "Pictoris", "Piscium", "Piscis Austrini", "Puppis", "Pyxidis",
        "Reticuli",
        "Sagittae", "Sagittarii", "Scorpii", "Sculptoris", "Scuti", "Serpentis", "Sextantis", "Tauri", "Telescopii",
        "Trianguli",
        "Trianguli Australis", "Tucanae", "Ursae Majoris", "Ursae Minoris", "Velorum", "Virginis", "Volantis",
        "Vulpeculae"
    };

static const std::string alphabeta[11] =
    {
        "Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Eta", "Theta", "Iota", "Kappa",
        "Lambda"
    };

static const std::string alphabeta_letter[11] =
    {
        "α", "β", "γ", "δ", "ε", "ζ", "η", "θ", "ι", "κ",
        "λ"
    };

static const std::string raw_star_names[425] =
    {
        "Acamar", "Achernar", "Achird", "Acrab", "Acrux", "Acubens", "Adhafera", "Adhara", "Adhil", "Agena",
        "Aladfar", "Albaldah", "Albali", "Albireo", "Alchiba", "Alcor", "Alcyone", "Alderamin", "Aldhibain", "Aldib",
        "Alfecca", "Alfirk", "Algedi", "Algenib", "Algenubi", "Algieba", "Algjebbath", "Algol", "Algomeyla", "Algorab",
        "Alhajoth", "Alhena", "Alifa", "Alioth", "Alkaid", "Alkalurops", "Alkaphrah", "Alkes", "Alkhiba", "Almach",
        "Almeisan", "Almuredin", "AlNa'ir", "Alnasl", "Alnilam", "Alnitak", "Alniyat", "Alphard", "Alphecca",
        "Alpheratz",
        "Alrakis", "Alrami", "Alrescha", "AlRijil", "Alsahm", "Alsciaukat", "Alshain", "Alshat", "Alshemali",
        "Alsuhail",
        "Altair", "Altais", "Alterf", "Althalimain", "AlTinnin", "Aludra", "AlulaAustralis", "AlulaBorealis", "Alwaid",
        "Alwazn",
        "Alya", "Alzirr", "AmazonStar", "Ancha", "Anchat", "AngelStern", "Angetenar", "Ankaa", "Anser", "Antecanis",
        "Apollo", "Arich", "Arided", "Arietis", "Arkab", "ArkebPrior", "Arneb", "Arrioph", "AsadAustralis", "Ascella",
        "Aschere", "AsellusAustralis", "AsellusBorealis", "AsellusPrimus", "Ashtaroth", "Asmidiske", "Aspidiske",
        "Asterion", "Asterope", "Asuia",
        "Athafiyy", "Atik", "Atlas", "Atria", "Auva", "Avior", "Azelfafage", "Azha", "Azimech", "BatenKaitos",
        "Becrux", "Beid", "Bellatrix", "Benatnasch", "Biham", "Botein", "Brachium", "Bunda", "Cajam", "Calbalakrab",
        "Calx", "Canicula", "Capella", "Caph", "Castor", "Castula", "Cebalrai", "Ceginus", "Celaeno", "Chara",
        "Chertan", "Choo", "Clava", "CorCaroli", "CorHydrae", "CorLeonis", "Cornu", "CorScorpii", "CorSepentis",
        "CorTauri",
        "Coxa", "Cursa", "Cymbae", "Cynosaura", "Dabih", "DenebAlgedi", "DenebDulfim", "DenebelOkab", "DenebKaitos",
        "DenebOkab",
        "Denebola", "Dhalim", "Dhur", "Diadem", "Difda", "DifdaalAuwel", "Dnoces", "Dubhe", "Dziban", "Dzuba",
        "Edasich", "ElAcola", "Elacrab", "Electra", "Elgebar", "Elgomaisa", "ElKaprah", "ElKaridab", "Elkeid",
        "ElKhereb",
        "Elmathalleth", "Elnath", "ElPhekrah", "Eltanin", "Enif", "Erakis", "Errai", "FalxItalica", "Fidis",
        "Fomalhaut",
        "Fornacis", "FumAlSamakah", "Furud", "Gacrux", "Gallina", "GarnetStar", "Gemma", "Genam", "Giausar",
        "GiedePrime",
        "Giedi", "Gienah", "Gienar", "Gildun", "Girtab", "Gnosia", "Gomeisa", "Gorgona", "Graffias", "Hadar",
        "Hamal", "Haris", "Hasseleh", "Hastorang", "Hatysa", "Heka", "Hercules", "Heze", "Hoedus", "Homam",
        "HyadumPrimus", "Icalurus", "Iclarkrav", "Izar", "Jabbah", "Jewel", "Jugum", "Juza", "Kabeleced", "Kaff",
        "Kaffa", "Kaffaljidma", "Kaitain", "KalbalAkrab", "Kat", "KausAustralis", "KausBorealis", "KausMedia", "Keid",
        "KeKouan",
        "Kelb", "Kerb", "Kerbel", "KiffaBoraelis", "Kitalpha", "Kochab", "Kornephoros", "Kraz", "Ksora", "Kuma",
        "Kurhah", "Kursa", "Lesath", "Maasym", "Maaz", "Mabsuthat", "Maia", "Marfik", "Markab", "Marrha",
        "Matar", "Mebsuta", "Megres", "Meissa", "Mekbuda", "Menkalinan", "Menkar", "Menkent", "Menkib", "Merak",
        "Meres", "Merga", "Meridiana", "Merope", "Mesartim", "Metallah", "Miaplacidus", "Mimosa", "Minelauva", "Minkar",
        "Mintaka", "Mirac", "Mirach", "Miram", "Mirfak", "Mirzam", "Misam", "Mismar", "Mizar", "Muhlifain",
        "Muliphein", "Muphrid", "Muscida", "NairalSaif", "NairalZaurak", "Naos", "Nash", "Nashira", "Navi", "Nekkar",
        "Nicolaus", "Nihal", "Nodus", "Nunki", "Nusakan", "OculusBoreus", "Okda", "Osiris", "OsPegasi", "Palilicium",
        "Peacock", "Phact", "Phecda", "Pherkad", "PherkadMinor", "Pherkard", "Phoenice", "Phurad", "Pishpai", "Pleione",
        "Polaris", "Pollux", "Porrima", "Postvarta", "Praecipua", "Procyon", "Propus", "Protrygetor", "Pulcherrima",
        "Rana",
        "RanaSecunda", "Rasalas", "Rasalgethi", "Rasalhague", "Rasalmothallah", "RasHammel", "Rastaban", "Reda",
        "Regor", "Regulus",
        "Rescha", "RigilKentaurus", "RiglalAwwa", "Rotanen", "Ruchba", "Ruchbah", "Rukbat", "Rutilicus", "Saak",
        "Sabik",
        "Sadachbia", "Sadalbari", "Sadalmelik", "Sadalsuud", "Sadatoni", "Sadira", "Sadr", "Saidak", "Saiph", "Salm",
        "Sargas", "Sarin", "Sartan", "Sceptrum", "Scheat", "Schedar", "Scheddi", "Schemali", "Scutulum", "SeatAlpheras",
        "Segin", "Seginus", "Shaula", "Shedir", "Sheliak", "Sheratan", "Singer", "Sirius", "Sirrah", "Situla",
        "Skat", "Spica", "Sterope", "Subra", "Suha", "Suhail", "SuhailHadar", "SuhailRadar", "Suhel", "Sulafat",
        "Superba", "Svalocin", "Syrma", "Tabit", "Tais", "Talitha", "TaniaAustralis", "TaniaBorealis", "Tarazed",
        "Tarf",
        "TaTsun", "Taygeta", "Tegmen", "Tejat", "TejatPrior", "Terebellum", "Theemim", "Thuban", "Tolimann",
        "Tramontana",
        "Tsih", "Tureis", "Unukalhai", "Vega", "Venabulum", "Venator", "Vendemiatrix", "Vespertilio", "Vildiur",
        "Vindemiatrix",
        "Wasat", "Wazn", "YedPosterior", "YedPrior", "Zaniah", "Zaurak", "Zavijava", "ZenithStar", "Zibel", "Zosma",
        "Zubenelakrab", "ZubenElgenubi", "Zubeneschamali", "ZubenHakrabi", "Zubra"
    };

static const std::string raw_giant_names[60] =
    {
        "AH Scorpii", "Aldebaran", "Alpha Herculis", "Antares", "Arcturus", "AV Persei", "BC Cygni", "Betelgeuse",
        "BI Cygni", "BO Carinae",
        "Canopus", "CE Tauri", "CK Carinae", "CW Leonis", "Deneb", "Epsilon Aurigae", "Eta Carinae", "EV Carinae",
        "IX Carinae", "KW Sagittarii",
        "KY Cygni", "Mira", "Mu Cephei", "NML Cygni", "NR Vulpeculae", "PZ Cassiopeiae", "R Doradus", "R Leporis",
        "Rho Cassiopeiae", "Rigel",
        "RS Persei", "RT Carinae", "RU Virginis", "RW Cephei", "S Cassiopeiae", "S Cephei", "S Doradus", "S Persei",
        "SU Persei", "TV Geminorum",
        "U Lacertae", "UY Scuti", "V1185 Scorpii", "V354 Cephei", "V355 Cepheus", "V382 Carinae", "V396 Centauri",
        "V437 Scuti", "V509 Cassiopeiae", "V528 Carinae",
        "V602 Carinae", "V648 Cassiopeiae", "V669 Cassiopeiae", "V838 Monocerotis", "V915 Scorpii", "VV Cephei",
        "VX Sagittarii", "VY Canis Majoris", "WOH G64", "XX Persei"
    };

static const std::string giant_name_formats[7] =
    {"HD {0:04}{1:02}", "HDE {0:04}{1:02}", "HR {0:04}", "HV {0:04}", "LBV {0:04}-{1:02}", "NSV {0:04}",
     "YSC {0:04}-{1:02}"};

static const std::string neutron_star_name_formats[2] = {"NTR J{0:02}{1:02}+{2:02}", "NTR J{0:02}{1:02}-{2:02}"};

static const std::string black_hole_name_formats[2] = {"DSR J{0:02}{1:02}+{2:02}", "DSR J{0:02}{1:02}-{2:02}"};

std::string NameGen::randomName(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = (int)(dotNet35Random.nextDouble() * 1.8 + 2.3);
    std::string text;
    for (int i = 0; i < num; i++) {
        if (!(dotNet35Random.nextDouble() < 0.05000000074505806) || i != 0) {
            text = ((!(dotNet35Random.nextDouble() < 0.97000002861022949) && num < 4) ? (text
                + con1[dotNet35Random.next(dim(con1))]) : (text + con0[dotNet35Random.next(dim(con0))]));
            text = ((i == num - 1 && dotNet35Random.nextDouble() < 0.89999997615814209) ? (text
                + ending[dotNet35Random.next(dim(ending))]) : ((!(dotNet35Random.nextDouble()
                < 0.97000002861022949)) ? (text + vow2[dotNet35Random.next(dim(vow2))]) : (text
                + vow1[dotNet35Random.next(dim(vow1))])));
        } else {
            text += vow0[dotNet35Random.next(dim(vow0))];
        }
    }

    strReplace(text, "uu", "u");
    strReplace(text, "ooo", "oo");
    strReplace(text, "eee", "ee");
    strReplace(text, "eea", "ea");
    strReplace(text, "aa", "a");
    strReplace(text, "yy", "y");
    text[0] = std::toupper(text[0]);
    return text;
}

std::string NameGen::randomStarName(int seed, Star *starData, Galaxy *galaxy) {
    DotNet35Random dotNet35Random(seed);
    int num = 0;
    while (num++ < 256) {
        std::string text = _randomStarName(dotNet35Random.next(), starData);
        bool flag = false;
        for (int i = 0; i < galaxy->starCount; i++) {
            if (galaxy->stars[i] != nullptr && galaxy->stars[i]->name == text) {
                flag = true;
                break;
            }
        }
        if (!flag) {
            return text;
        }
    }
    return "XStar";
}

std::string NameGen::_randomStarName(int seed, Star *starData) {
    DotNet35Random dotNet35Random(seed);
    int seed2 = dotNet35Random.next();
    double num = dotNet35Random.nextDouble();
    double num2 = dotNet35Random.nextDouble();
    if (starData->type == EStarType::GiantStar) {
        if (num2 < 0.40000000596046448) {
            return randomGiantStarNameFromRawNames(seed2);
        }
        if (num2 < 0.699999988079071) {
            return randomGiantStarNameWithConstellationAlpha(seed2);
        }
        return randomGiantStarNameWithFormat(seed2);
    }
    if (starData->type == EStarType::NeutronStar) {
        return randomNeutronStarNameWithFormat(seed2);
    }
    if (starData->type == EStarType::BlackHole) {
        return randomBlackHoleNameWithFormat(seed2);
    }
    if (num < 0.60000002384185791) {
        return randomStarNameFromRawNames(seed2);
    }
    if (num < 0.93000000715255737) {
        return randomStarNameWithConstellationAlpha(seed2);
    }
    return randomStarNameWithConstellationNumber(seed2);
}

std::string NameGen::randomStarNameFromRawNames(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    num %= dim(raw_star_names);
    return raw_star_names[num];
}

std::string NameGen::randomStarNameWithConstellationAlpha(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next();
    num %= dim(constellations);
    num2 %= dim(alphabeta);
    std::string text = constellations[num];
    if (text.length() > 10) {
        return alphabeta_letter[num2] + " " + text;
    }
    return alphabeta[num2] + " " + text;
}

std::string NameGen::randomStarNameWithConstellationNumber(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next(27, 75);
    num %= dim(constellations);
    return std::to_string(num2) + ' ' + constellations[num];
}

std::string NameGen::randomGiantStarNameFromRawNames(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    num %= dim(raw_giant_names);
    return raw_giant_names[num];
}

std::string NameGen::randomGiantStarNameWithConstellationAlpha(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next(15, 26);
    int num3 = dotNet35Random.next(0, 26);
    num %= dim(constellations);
    int num4 = (65 + num2);
    int num5 = (65 + num3);
    return std::to_string(num4 + num5) + ' ' + constellations[num];
}

std::string NameGen::randomGiantStarNameWithFormat(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next(10000);
    int num3 = dotNet35Random.next(100);
    num %= dim(giant_name_formats);
    return fmt::format(giant_name_formats[num], num2, num3);
}

std::string NameGen::randomNeutronStarNameWithFormat(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next(24);
    int num3 = dotNet35Random.next(60);
    int num4 = dotNet35Random.next(0, 60);
    num %= dim(neutron_star_name_formats);
    return fmt::format(neutron_star_name_formats[num], num2, num3, num4);
}

std::string NameGen::randomBlackHoleNameWithFormat(int seed) {
    DotNet35Random dotNet35Random(seed);
    int num = dotNet35Random.next();
    int num2 = dotNet35Random.next(24);
    int num3 = dotNet35Random.next(60);
    int num4 = dotNet35Random.next(0, 60);
    num %= dim(black_hole_name_formats);
    return fmt::format(black_hole_name_formats[num], num2, num3, num4);
}
