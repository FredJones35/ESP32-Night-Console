// Prosedürel duvar dokuları (enter'da heap'e üretilir, exit'te bırakılır)
#pragma once
#include <stdint.h>

namespace doom {

static const int TEX = 32;

enum TexId {
  T_BRICK, T_METAL, T_FLESH, T_STONE, T_HELL, T_TECH,   // haritada 1..6
  T_EXIT,                                               // X
  T_DOOR, T_DOOR_R, T_DOOR_Y, T_DOOR_B,
  T_COUNT
};

void genTextures(uint16_t* tex);   // T_COUNT * TEX * TEX piksel

}  // namespace doom
