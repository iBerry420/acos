#pragma once

#include <chrono>
#include <filesystem>

namespace avacli {

inline long long fileTimeToUnixSeconds(std::filesystem::file_time_type ft) {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L && !defined(__APPLE__)
    auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::clock_cast<std::chrono::system_clock>(ft));
    return sctp.time_since_epoch().count();
#else
    // Portable fallback: compute delta from file_clock::now() and apply to system_clock::now()
    auto delta = ft - std::filesystem::file_time_type::clock::now();
    auto sysTime = std::chrono::system_clock::now() + delta;
    return std::chrono::time_point_cast<std::chrono::seconds>(sysTime)
        .time_since_epoch()
        .count();
#endif
}

} // namespace avacli
