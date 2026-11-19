/*
 * types.h - data types of MiniJava.
 */
#pragma once

#include <string>

/* Error marks an expression whose type could not be determined, so one
 * mistake does not cause a chain of follow-on errors. */
enum class Type { Int, Float, Bool, Error };

inline std::string typeName(Type t) {
    switch (t) {
    case Type::Int:   return "int";
    case Type::Float: return "float";
    case Type::Bool:  return "boolean";
    case Type::Error: return "error";
    }
    return "unknown";
}
