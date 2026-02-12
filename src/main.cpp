#include "game.h"

// Forward declarations for callbacks expected by game class but defined in
// global scope or static Actually game.cpp implements them inside, but if they
// need to be passed to C-API... In game.cpp I used members but
// wgpuInstanceRequestAdapter header needs free function. Let's rely on game.cpp
// implementation details.

int main() {
  Game game;
  if (game.Initialize()) {
    game.Run();
  }
  return 0;
}
