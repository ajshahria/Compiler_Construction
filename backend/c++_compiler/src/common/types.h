// types.h - the three data types of the language, shared by every phase.
#ifndef TYPES_H
#define TYPES_H

#include <string>

enum Type { TY_INT, TY_FLOAT, TY_BOOL, TY_ERROR, TY_VOID };   // TY_VOID: functions only

inline const char* typeName(Type t) {
    switch (t) {
        case TY_INT:   return "int";
        case TY_FLOAT: return "float";
        case TY_BOOL:  return "bool";
        case TY_VOID:  return "void";
        default:       return "<error>";
    }
}

inline bool isNumeric(Type t) { return t == TY_INT || t == TY_FLOAT; }

#endif
