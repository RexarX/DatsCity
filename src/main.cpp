#include "Application.h"

int main(int argc, char** argv) {
  app::Application app("Game");
  app.ConnectToServer("https://games-test.datsteam.dev/play/placeholder", "token");
  app.Run();

  return 0;
}