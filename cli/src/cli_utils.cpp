#include "../../src/include/flowhook_core.h"
#include <filesystem>
#include "include/cli_utils.h"


namespace fs = std::filesystem;
using namespace flowhook;

namespace flowhook_cli {
    fs::path get_home_directory() {
    #if defined(_WIN32) || defined(_WIN64)
        // Windows environment
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile) {
            return fs::path(userProfile);
        }

        // Fallback for older Windows environments
        const char* homeDrive = std::getenv("HOMEDRIVE");
        const char* homePath = std::getenv("HOMEPATH");
        if (homeDrive && homePath) {
            return fs::path(homeDrive) / homePath;
        }
    #else
        // Linux, WSL, macOS, and other UNIX-like systems
        const char* home = std::getenv("HOME");
        if (home) {
            return fs::path(home);
        }
    #endif

        // Return an empty path if nothing was found
        return fs::path();
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
        auto fh = _fh.unwrap();
        return fh;
    }

}
