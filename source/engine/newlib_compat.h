#pragma once
//
// PS3 port shim — force-included into every C++ translation unit (see Makefile
// CXXFLAGS: -include newlib_compat.h).
//
// PSL1GHT's libstdc++ is backed by a newlib built without full C99 stdio, so it
// compiles out std::to_string / std::stoi & friends (they sit behind
// _GLIBCXX_USE_C99_STDIO in <string>). The reused monopoly engine (Player.h) and
// the vendored nlohmann/json.hpp both call std::to_string, so we supply it here
// rather than patching those sources. When the toolchain *does* provide them
// (guard below), this header is inert.
//
#include <string>

#if !defined(_GLIBCXX_USE_C99_STDIO) || (_GLIBCXX_USE_C99_STDIO == 0)
#include <stdio.h>   // newlib declares snprintf in the GLOBAL namespace, not std::
#include <stdlib.h>

// The vendored nlohmann/json.hpp calls these C library functions as std::… but
// this newlib libstdc++ leaves them only in the global namespace. Pull them in.
namespace std {
    using ::snprintf;
    using ::vsnprintf;
    using ::strtof;
    using ::strtod;
    using ::strtold;
    using ::strtol;
    using ::strtoul;
    using ::strtoll;
    using ::strtoull;
}

namespace std {
    inline std::string to_string(int v)                { char b[32];  ::snprintf(b, sizeof b, "%d",   v); return b; }
    inline std::string to_string(unsigned v)           { char b[32];  ::snprintf(b, sizeof b, "%u",   v); return b; }
    inline std::string to_string(long v)               { char b[32];  ::snprintf(b, sizeof b, "%ld",  v); return b; }
    inline std::string to_string(unsigned long v)      { char b[32];  ::snprintf(b, sizeof b, "%lu",  v); return b; }
    inline std::string to_string(long long v)          { char b[32];  ::snprintf(b, sizeof b, "%lld", v); return b; }
    inline std::string to_string(unsigned long long v) { char b[32];  ::snprintf(b, sizeof b, "%llu", v); return b; }
    inline std::string to_string(float v)              { char b[64];  ::snprintf(b, sizeof b, "%f",   (double)v); return b; }
    inline std::string to_string(double v)             { char b[64];  ::snprintf(b, sizeof b, "%f",   v); return b; }
    inline std::string to_string(long double v)        { char b[64];  ::snprintf(b, sizeof b, "%Lf",  v); return b; }
}
#endif
