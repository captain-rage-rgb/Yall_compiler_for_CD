#ifndef YALL_NUMERIC_H
#define YALL_NUMERIC_H

#include <cstdint>
#include <cmath>
#include <limits>

/**
 * @file numeric.h
 * @brief Shared numeric semantics for Yall compiler (used by Optimizer and VM).
 *
 * Rules:
 * - num is int64_t.
 * - +, -, *, unary - wrap in two's complement via uint64_t to prevent C++ UB.
 * - Integer division truncates toward zero.
 * - INT64_MIN / -1 gives INT64_MIN; INT64_MIN % -1 gives 0.
 * - Modulo takes the sign of the dividend.
 * - safeDiv and safeMod return false on divisor == 0.
 * - decimal is an IEEE double. Division by 0.0 is a runtime error (never folded).
 * - num -> decimal is static_cast<double>.
 */

inline int64_t wrapAdd(int64_t a, int64_t b) {
    uint64_t ua = static_cast<uint64_t>(a);
    uint64_t ub = static_cast<uint64_t>(b);
    return static_cast<int64_t>(ua + ub);
}

inline int64_t wrapSub(int64_t a, int64_t b) {
    uint64_t ua = static_cast<uint64_t>(a);
    uint64_t ub = static_cast<uint64_t>(b);
    return static_cast<int64_t>(ua - ub);
}

inline int64_t wrapMul(int64_t a, int64_t b) {
    uint64_t ua = static_cast<uint64_t>(a);
    uint64_t ub = static_cast<uint64_t>(b);
    return static_cast<int64_t>(ua * ub);
}

inline int64_t wrapNeg(int64_t a) {
    uint64_t ua = static_cast<uint64_t>(a);
    return static_cast<int64_t>(~ua + 1ULL);
}

inline bool safeDiv(int64_t a, int64_t b, int64_t& out) {
    if (b == 0) return false;
    if (a == std::numeric_limits<int64_t>::min() && b == -1) {
        out = std::numeric_limits<int64_t>::min(); // Prevent C++ SIGFPE trap
        return true;
    }
    out = a / b;
    return true;
}

inline bool safeMod(int64_t a, int64_t b, int64_t& out) {
    if (b == 0) return false;
    if (a == std::numeric_limits<int64_t>::min() && b == -1) {
        out = 0; // Prevent C++ SIGFPE trap
        return true;
    }
    out = a % b;
    return true;
}

#endif // YALL_NUMERIC_H
