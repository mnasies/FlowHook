#include "../../src/include/flowhook_core.h"
#include "./include/CLI11.hpp"
#include <iostream>
#include <unistd.h>
#include <filesystem>
#include <cstdlib>
#include <csignal>


#include "../../src/include/macros.hpp"
#include "version.h"

namespace flowhook_cli {
    void register_add(CLI::App*, flowhook::FlowHookCore*);
    void register_run(CLI::App*, flowhook::FlowHookCore*);
    void register_init(CLI::App*, flowhook::FlowHookCore*);
    void register_list(CLI::App*, flowhook::FlowHookCore*);
    void register_remove(CLI::App*, flowhook::FlowHookCore*);
    void register_set(CLI::App*, flowhook::FlowHookCore*);
    void register_check(CLI::App*, flowhook::FlowHookCore*);
}

namespace fs = std::filesystem;
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

FlowHookCore * handle_fh_error(Result<FlowHookCore *> _fh) {
    if (_fh.isErr()) {
        if(_fh.getErrCode() == ErrorCode::CONFIG_PARSE_FAILED)
        {
            std::cerr << "Config file is corrupted." << std::endl;
            fs::path config_path = get_home_directory() / ".config" / "flowhook"/ "config.json";
            std::cout << "Path : " << config_path << std::endl;
            std::cerr << "Options: " << std::endl;
            std::cout << " [1] clear all data and start fresh" << std::endl;
            std::cout << " [2] exit and fix manually" << std::endl;

            std::cout << "Enter your choice: ";
            int choice;
            std::cin >> choice;
            if (choice == 1)
            {
                fs::remove(config_path);
                std::cout << "Config file cleared. Starting fresh..." << std::endl;
                return nullptr;
            }
            else if (choice == 2)
                return nullptr;
            else
                std::cerr << "Invalid choice." << std::endl;
            return nullptr;
        }
        else
        {
            std::cerr << "Failed to create FlowHook instance: " << _fh.getErrMessage()
                << std::endl;
            return nullptr;
        }
    }
    fh = _fh.unwrap();
    return fh;
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
