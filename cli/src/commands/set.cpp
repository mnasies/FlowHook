#include "../../src/include/flowhook_core.h"
#include "../include/CLI11.hpp"
#include "../../src/include/macros.hpp"

#include <climits>


namespace fs = std::filesystem;

namespace flowhook_cli {
    void register_set(CLI::App* app, flowhook::FlowHookCore* fh)
    {
        auto* set = app->add_subcommand("set", "setting a property for the task");

        // this is a purely ceremonial flag(so that CLI11 won't say there is unrecognized flag)
        // the actual parsing of verbose and setting of the environment variable is done in main.cpp
        set->add_flag("--debug", FLOWHOOK_DEBUG, "Enable debug output. \n// is an extremely detailed output that shows every step of the process.");
        set->add_flag("--verbose", FLOWHOOK_VERBOSE, "Enable verbose output. \n// shows a summary of the process.");


        static bool active = false;
        set->add_flag("--active", active, "Set task as active \n // enables you to run specified tasks with the `run active` command");

        static int timeout = INT_MIN;
        set->add_option("--timeout", timeout, "Set the timeout for your commands in seconds \n // if the command takes longer than the specified time, it will be terminated.");

        static bool deactive = false;
        set->add_flag("--deactive", deactive, "Set a task enables active as deactive");

        static int depth = INT_MIN;
        set->add_option("--depth", depth, "Set the depth to watch from your working directory.");

        set->callback([=]() mutable {
          std::string set_task_id = fs::current_path().string();

          if (active) {
            auto r = fh->activate_task(set_task_id);
            if (r.isErr()) {
              std::cout << "Failed to set task as active: " << r.getErrMessage()
                        << std::endl;
              return;
            }
            std::cout << "Task " << set_task_id << " is now active" << std::endl;
          }
          if(deactive) {
              auto r = fh->deactivate_task(set_task_id);
              if (r.isErr()) {
                std::cout << "Failed to set task as deactive: " << r.getErrMessage()
                          << std::endl;
                return;
              }
              std::cout << "Task " << set_task_id << " is now deactive" << std::endl;
          }
          if(depth != INT_MIN) {
            auto r = fh->set_depth(set_task_id, depth);
            if (r.isErr()) {
              std::cout << "Failed to set depth: " << r.getErrMessage()
                        << std::endl;
              return;
            }
            std::cout << "Depth set to " << depth << std::endl;
          }
          if(timeout != INT_MIN)
          {
            auto r = fh->set_timeout(set_task_id, timeout);
            if (r.isErr()) {
              std::cout << "Failed to set timeout: " << r.getErrMessage()
                        << std::endl;
              return;
            }
            std::cout << "Timeout set to " << timeout << "s" << std::endl;
          }
        });
    }
}
