#pragma once

#include <ctime>

namespace avacli {

inline void utcTmFromTime(std::time_t t, std::tm* out) {
#if defined(_WIN32)
    gmtime_s(out, &t);
#else
    gmtime_r(&t, out);
#endif
}

} // namespace avacli
