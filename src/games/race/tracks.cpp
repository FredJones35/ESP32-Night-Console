// Firmware'e gömülü pistler. SD karttaki .trk dosyalarıyla aynı formattadır
// (bkz. docs/LEVEL_FORMAT.md). Ek pistler SD kartın /race klasöründen yüklenir.
#include "tracks.h"

namespace race {

static const char TRACK1[] = R"(name=Sahil Yolu
laps=3
sky=day
theme=grass
skill=0.86
deco=tree,bush,palm
density=0.45
[road]
# uzunluk viraj yokus
30 0 0
40 2 0
30 0 500
50 -3 0
30 0 -500
40 4 700
30 0 -700
60 -2 0
25 0 0
40 3 -400
30 0 400
50 -4 0
30 0 0
[objects]
# segment yan tur
40 -2.2 billboard
120 2.2 house
200 -2.4 house
300 2.0 billboard
)";

static const char TRACK2[] = R"(name=Col Firtinasi
laps=3
sky=dusk
theme=desert
skill=0.95
deco=cactus,rock,palm
density=0.4
[road]
30 0 0
30 0 1200
40 -3 0
30 3 -1200
40 0 800
50 5 0
30 0 -800
40 -5 600
40 0 -600
30 2 0
40 -2 1000
40 0 -1000
60 4 0
30 0 0
[objects]
60 -2.0 billboard
260 2.4 rock
400 -2.2 billboard
)";

const char* const BUILTIN_TRACKS[] = { TRACK1, TRACK2 };
const int BUILTIN_COUNT = 2;

}  // namespace race
