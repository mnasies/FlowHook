#include <filesystem>
#include <fstream>
#include <random>

#include "include/identity.h"
#include "json.hpp"

namespace fs = std::filesystem;

namespace flowhook {

std::string marker_path(const std::string &dir) {
    return (fs::path(dir) / MARKER_FILE_NAME).string();
}

std::string generate_uuid() {
    const char *hex = "0123456789abcdef";
    std::random_device rd;
    // seed with multiple device words; a single-word seed repeats when the
    // platform falls back to a deterministic random_device implementation
    std::seed_seq seed64{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    std::mt19937_64 rng(seed64);
    unsigned char bytes[16];
    for (auto &b : bytes)
        b = static_cast<unsigned char>(rng() & 0xff);

    // set uuid v4 version and variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;

    std::string uuid;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            uuid += '-';
        uuid += hex[bytes[i] >> 4];
        uuid += hex[bytes[i] & 0x0f];
    }
    return uuid;
}

Result<void> write_marker(const std::string &dir, const Task &task) {
    if (dir.empty()) {
        return Result<void>::Err(FWError::make(
            ErrorCode::EMPTY_VALUE, "Error: marker directory is empty ✗"));
    }
    if (!fs::is_directory(dir)) {
        return Result<void>::Err(FWError::make(
            ErrorCode::PATH_NOT_FOUND, "Error: marker directory " + dir + " does not exist ✗"));
    }

    nlohmann::json marker;
    marker["version"] = MARKER_VERSION;
    marker["uuid"] = task.uuid;
    marker["task_name"] = task.name;

    std::ofstream file(marker_path(dir), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::Err(FWError::make(
            ErrorCode::SYS_IO_FAILED, "Error: opening identity marker for writing ✗"));
    }
    file << marker.dump(4) << std::endl;
    if (file.fail()) {
        return Result<void>::Err(FWError::make(
            ErrorCode::SYS_IO_FAILED, "Error: writing identity marker failed ✗"));
    }
    return Result<void>::Ok();
}

Result<Identity> read_marker(const std::string &dir) {
    if (dir.empty()) {
        return Result<Identity>::Err(FWError::make(
            ErrorCode::EMPTY_VALUE, "Error: marker directory is empty ✗"));
    }
    std::ifstream file(marker_path(dir), std::ios::in);
    if (!file.is_open()) {
        return Result<Identity>::Err(FWError::make(
            ErrorCode::PATH_NOT_FOUND, "Error: no flowhook identity marker in " + dir + " ✗"));
    }
    try {
        nlohmann::json marker = nlohmann::json::parse(file);
        Identity identity;
        identity.uuid = marker.value("uuid", "");
        identity.task_name = marker.value("task_name", "");
        if (identity.isNull()) {
            return Result<Identity>::Err(FWError::make(
                ErrorCode::CONFIG_PARSE_FAILED, "Error: identity marker has no uuid ✗"));
        }
        return Result<Identity>::Ok(identity);
    }
    catch (const nlohmann::json::parse_error &) {
        return Result<Identity>::Err(FWError::make(
            ErrorCode::CONFIG_PARSE_FAILED, "Error: identity marker is corrupted ✗"));
    }
}

Result<void> delete_marker(const std::string &dir) {
    std::error_code ec;
    fs::remove(marker_path(dir), ec);
    if (ec) {
        return Result<void>::Err(FWError::make(
            ErrorCode::SYS_IO_FAILED, "Error: deleting identity marker failed ✗"));
    }
    return Result<void>::Ok();
}

} // namespace flowhook
