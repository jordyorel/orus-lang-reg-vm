#ifndef ORUS_VM_STRING_OPS_H
#define ORUS_VM_STRING_OPS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct ObjString;
typedef struct ObjString ObjString;
typedef struct HashMap HashMap;

// StringBuilder facilitates efficient string concatenation.
typedef struct {
    char* buffer;
    size_t capacity;
    size_t length;
} StringBuilder;

// Rope node kinds for zero-copy strings
typedef enum {
    ROPE_LEAF,
    ROPE_CONCAT,
    ROPE_SUBSTRING
} RopeKind;

typedef struct StringRope {
    RopeKind kind;
    union {
        struct {
            char* data;
            size_t len;
            bool is_ascii;
            bool is_interned;
        } leaf;
        struct {
            struct StringRope* left;
            struct StringRope* right;
            size_t total_len;
            uint32_t depth;
        } concat;
        struct {
            struct StringRope* base;
            size_t start;
            size_t len;
        } substring;
    } as;
    uint32_t hash_cache;
    bool hash_valid;
} StringRope;

typedef struct {
    HashMap* interned;
    size_t threshold;
    size_t total_interned;
} StringInternTable;

extern StringInternTable globalStringTable;

StringBuilder* createStringBuilder(size_t initial_capacity);
void appendToStringBuilder(StringBuilder* sb, const char* str, size_t len);
ObjString* stringBuilderToString(StringBuilder* sb);
void freeStringBuilder(StringBuilder* sb);

// Rope helpers
StringRope* rope_from_cstr(const char* str, size_t len);
char* rope_to_cstr(StringRope* rope);

// Interning
void init_string_table(StringInternTable* table);
ObjString* intern_string(const char* chars, int length);

// Cleanup routines
void free_rope(StringRope* rope);
void free_string_table(StringInternTable* table);

#endif // ORUS_VM_STRING_OPS_H
