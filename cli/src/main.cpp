#include "../../src/include/flowhook_core.h"
#include "./include/CLI11.hpp"
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <csignal>


#include "../../src/include/macros.hpp"
#include "version.h"

namespace flowhook_cli {
    void register_add(CLI::App*, flowhook::FlowHookCore*&);
    void register_run(CLI::App*, flowhook::FlowHookCore*&);
    void register_init(CLI::App*, flowhook::FlowHookCore*&);
    void register_list(CLI::App*, flowhook::FlowHookCore*&);
    void register_remove(CLI::App*, flowhook::FlowHookCore*&);
    void register_set(CLI::App*, flowhook::FlowHookCore*&);
    void register_check(CLI::App*, flowhook::FlowHookCore*&);
}

using namespace flowhook;
bool FLOWHOOK_VERBOSE = false;
bool FLOWHOOK_DEBUG = false;
bool FLOWHOOK_QUIET = false;

static FlowHookCore* fh = nullptr;

void handle_sigint(int)
{
    if(fh) fh->stop_all();
    std::cout << "Exiting safely..." << std::endl;
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_sigint);

  for (int i = 1; i < argc; i++)
  {
      if (std::string(argv[i]) == "--debug")
      {
          FLOWHOOK_DEBUG = true;
          FLOWHOOK_VERBOSE = true;
      }
      if (std::string(argv[i]) == "--verbose")
          FLOWHOOK_VERBOSE = true;

      if (std::string(argv[i]) == "--quiet")
          FLOWHOOK_QUIET = true;

      if (std::string(argv[i]) == "--version")
      {
          std::cout << "FlowHook version " << FLOWHOOK_VERSION << std::endl;
          return 0;
      }
  }

  CLI::App app{"FlowHook"};
  app.require_subcommand(0, 1);

  bool version_flag = false;
  app.add_flag("--version", version_flag, "Print version information");

  flowhook_cli::register_init(&app, fh);
  flowhook_cli::register_add(&app, fh);
  flowhook_cli::register_run(&app, fh);
  flowhook_cli::register_list(&app, fh);
  flowhook_cli::register_remove(&app, fh);
  flowhook_cli::register_set(&app, fh);
  flowhook_cli::register_check(&app, fh);


  CLI11_PARSE(app, argc, argv);
  delete fh;
  return 0;
}
