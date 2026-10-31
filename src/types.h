#ifndef YALL_TYPES_H
#define YALL_TYPES_H

#include <string>

/**
 * @enum Type
 * @brief Represents the data types supported in the Yall type system.
 *
 * Types:
 *  - NUM: 64-bit signed integer.
 *  - DECIMAL: double-precision floating point.
 *  - LETTER: single ASCII character.
 *  - TEXT: dynamically-sized string.
 *  - FLAG: boolean value ('yes' / 'no').
 *  - NOTHING: void type (valid for function return types only).
 *  - ERROR: sentinel type used to suppress cascading error reports.
 */
enum class Type {
    NUM,
    DECIMAL,
    LETTER,
    TEXT,
    FLAG,
    NOTHING,
    ERROR
};

/**
 * @brief Converts a Type enum value to its language keyword string.
 */
inline std::string typeToString(Type t) {
    switch (t) {
        case Type::NUM:     return "num";
        case Type::DECIMAL: return "decimal";
        case Type::LETTER:  return "letter";
        case Type::TEXT:    return "text";
        case Type::FLAG:    return "flag";
        case Type::NOTHING: return "nothing";
        case Type::ERROR:   return "error";
    }
    return "unknown";
}

/**
 * @brief Converts a type keyword string to its corresponding Type enum value.
 */
inline Type stringToType(const std::string& str) {
    if (str == "num")     return Type::NUM;
    if (str == "decimal") return Type::DECIMAL;
    if (str == "letter")  return Type::LETTER;
    if (str == "text")    return Type::TEXT;
    if (str == "flag")    return Type::FLAG;
    if (str == "nothing") return Type::NOTHING;
    return Type::ERROR;
}

/**
 * @brief Checks if a type is numeric (num or decimal).
 */
inline bool isNumeric(Type t) {
    return t == Type::NUM || t == Type::DECIMAL;
}

/**
 * @brief Checks if a source type is assignable to a target type.
 *
 * Rule 6: Identical types are assignable, or target is decimal and source is num
 * (implicit widening). Sentinel type ERROR is treated as universally assignable
 * to prevent cascading diagnostics.
 */
inline bool isAssignable(Type target, Type source) {
    if (target == Type::ERROR || source == Type::ERROR) return true;
    if (target == source) return true;
    if (target == Type::DECIMAL && source == Type::NUM) return true;
    return false;
}

#endif // YALL_TYPES_H
