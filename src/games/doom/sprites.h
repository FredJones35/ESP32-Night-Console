// Doom sprite verileri (flash'ta)
#pragma once

namespace doom {

struct Art {
  const char* const* rows;
  int w, h;
};

enum ArtId {
  A_ZOMBIE, A_ZOMBIE_ATK, A_ZOMBIE_DEAD,
  A_DEMON, A_DEMON_ATK, A_DEMON_DEAD,
  A_CASTER, A_CASTER_ATK, A_CASTER_DEAD,
  A_BOSS, A_BOSS_ATK, A_BOSS_DEAD,
  A_MEDKIT, A_ARMOR, A_BULLETS, A_SHELLS, A_SHOTGUN, A_MG, A_KEY, A_TORCH,
  A_COUNT
};

const Art& art(ArtId id);

}  // namespace doom
