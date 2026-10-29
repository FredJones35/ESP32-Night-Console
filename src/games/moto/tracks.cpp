// Firmware'e gömülü motokros parkurları. SD karttaki .mtb dosyalarıyla aynı formattadır
// (bkz. docs/LEVEL_FORMAT.md). Ek parkurlar SD kartın /moto klasöründen yüklenir.
#include "tracks.h"

namespace moto {

static const char TRACK1[] = R"(name=Isinma Turu
theme=forest
gold=30
silver=36
bronze=46
[terrain]
# komut parametreler (metre)
flat 20
hill 24 3
flat 10
bumps 24 0.8 4
up 16 3
flat 8
down 16 3
ramp 10 2.5
flat 22
hill 30 5
flat 10
bumps 30 1 5
valley 24 3
flat 16
ramp 12 3
flat 24
up 20 4
hill 16 2
down 20 4
flat 20
[objects]
# x tur
40 tree
75 sign
110 checkpoint
150 tree
190 rock
230 checkpoint
260 tree
300 flag
)";

static const char TRACK2[] = R"(name=Kum Tepeleri
theme=desert
gold=38
silver=45
bronze=56
[terrain]
flat 18
bumps 30 1.2 4
ramp 12 3
flat 18
hill 36 7
flat 6
gap 5 3
flat 16
up 24 6
ramp 8 2
down 26 8
flat 12
bumps 40 1.6 6
ramp 14 4
flat 28
valley 30 5
hill 20 3
flat 14
ramp 10 3
flat 14
gap 4 3
flat 30
[objects]
30 cactus
70 rock
120 checkpoint
160 cactus
210 checkpoint
250 rock
300 checkpoint
330 cactus
370 checkpoint
)";

const char* const BUILTIN_TRACKS[] = { TRACK1, TRACK2 };
const int BUILTIN_COUNT = 2;

}  // namespace moto
