// Menüde görünen oyun listesi
#include "../core/game.h"

extern const Game DOOM_GAME;
extern const Game SNAKE_GAME;
extern const Game TETRIS_GAME;
extern const Game FLAPPY_GAME;

const Game* const GAMES[] = {
  &DOOM_GAME,
  &SNAKE_GAME,
  &TETRIS_GAME,
  &FLAPPY_GAME,
};
const int GAME_COUNT = sizeof(GAMES) / sizeof(GAMES[0]);
