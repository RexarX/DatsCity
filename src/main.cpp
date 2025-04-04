#include "Application.h"

int main(int argc, char** argv) {
  app::Log::Init();
  app::Application app("Game");
  app.ConnectToServer("https://games-test.datsteam.dev/api/", "4bdbb30c-e152-430f-aaab-4e0e2f5b9d5b");
  app.Run();

  return 0;
}