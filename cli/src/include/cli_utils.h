#pragma once
#include "../../src/include/flowhook_core.h"
#include <filesystem>


namespace fs = std::filesystem;

namespace flowhook_cli {
    fs::path get_home_directory();
    flowhook::FlowHookCore * handle_fh_error(flowhook::Result<flowhook::FlowHookCore *> _fh);

}
