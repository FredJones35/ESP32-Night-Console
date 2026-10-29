// Firmware'e gömülü Doom bölümleri. SD karttaki .lvl dosyalarıyla aynı formattadır
// (bkz. docs/LEVEL_FORMAT.md). Ek bölümler SD kartın /doom klasöründen yüklenir.
#include "levels_builtin.h"

namespace doom {

static const char E1M1[] = R"(name=Giris Kapisi
par=90
light=190
fog=45
fogcolor=40,0,0
flicker=1
angle=0
[map]
111111111111111111111111
1t...t1m.......m1h....h1
1.....1...z...z.1..z...1
1..@..D..2.2.2..D.r..d.1
1.....1.........1......1
1t...t1..2.2.2..1z....z1
11S1111111.1111111111111
1a.h111111.1111111111111
1.m.111111.1111111111111
1111111111D1111111111111
1t.......z.....t1......1
1..22......22...1......1
1.z...d.....z...R..h...X
1..22......22...1......1
1t...m.........t1..m...1
111111111111111111111111
)";

static const char E1M2[] = R"(name=Kanli Laboratuvar
par=150
light=170
fog=55
fogcolor=45,0,8
flicker=1
angle=0
[map]
111111111111111111111111
1t...t3t...f....t3..h..3
1.....3..3....3..3.d...3
1.@...D...z..z...3..b..3
1m...h3..........3d...d3
1t...t3....s.....Y.....3
3333333..3....3..3.....3
1.a.h.3....f.....3..e..3
1..g..S..........3.....3
1.m.m.3..z....z..3h.t.h3
333333333333D33333333333
1t...t4.....d..........4
1.....4..44....44...44.4
1..y..D......d.....z...4
1.....4..44....44...44.4
1t.h.t4...z.......d....4
444444444444B44444444444
5t.........t..........t5
5..f....3.....3....f...5
5....z.....d.......z...X
5.......3.....3....e..h5
5t.........t..........t5
555555555555555555555555
)";

const char* const BUILTIN_LEVELS[] = { E1M1, E1M2 };
const int BUILTIN_COUNT = 2;

}  // namespace doom
