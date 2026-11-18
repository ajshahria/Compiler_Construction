
#pragma once

#include <string>

enum class Type {
    Int,
    Float,
    Double,
    Char,
    Bool,
    String,
    Void,
    Error
};

inline std::string typeName(Type t) {

    switch (t) {

        case Type::Int:
            return "int";

        case Type::Float:
            return "float";

        case Type::Double:
            return "double";

        case Type::Char:
            return "char";

        case Type::Bool:
            return "boolean";

        case Type::String:
            return "String";

        case Type::Void:
            return "void";

        case Type::Error:
            return "error";
    }

    return "unknown";
}
