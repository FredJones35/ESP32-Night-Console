#!/bin/sh
# Simülatörü derler: tools/sim/sim
set -e
cd "$(dirname "$0")/../.."
g++ -std=gnu++17 -O2 -g -Wall -Wextra -Wno-unused-parameter -DHOST -Isrc \
  src/core/gfx.cpp src/core/input.cpp src/core/parse.cpp src/core/levels.cpp \
  src/core/ui.cpp src/core/app.cpp src/menu/menu.cpp \
  $(find src/games -name '*.cpp') tools/sim/host.cpp -o tools/sim/sim "$@"
