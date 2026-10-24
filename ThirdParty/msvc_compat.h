// msvc_compat.h — force-included into the sound library on Windows only
// (see CMakeLists.txt), so the vendored module code builds with MSVC
// unchanged.
#pragma once

// M_PI and friends are only defined by MSVC's <cmath> when asked for, and the
// request has to come before the header is first included anywhere.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

// GCC's function attributes (always_inline hints in stmlib) mean nothing to
// MSVC; drop them.
#ifndef __GNUC__
#define __attribute__(x)
#endif
