#pragma once
#include <string>
enum class Type { Int, Float, Bool, Error };
inline std::string typeName(Type t){switch(t){case Type::Int:return "int";case Type::Float:return "float";case Type::Bool:return "boolean";default:return "<error>";}}
