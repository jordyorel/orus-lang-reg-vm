#include <stddef.h>
// Forward declarations for HashMap and HashMapEntry
typedef struct HashMap HashMap;
typedef struct HashMapEntry HashMapEntry;
// HashMapEntry definition (copied from type_representation.c)
struct HashMapEntry {
    int key;
    void* value;
    struct HashMapEntry* next;
};
// HashMap definition (copied from type_representation.c)
struct HashMap {
    HashMapEntry** buckets;
    size_t capacity;
    size_t count;
};
// vm_string_ops.c - String operations and optimizations


#include <stddef.h>
#include "vm/vm_string_ops.h"
#include "runtime/memory.h"
#include "vm/vm_constants.h"
#include "type/type.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations for HashMap and HashMapEntry
typedef struct HashMap HashMap;
typedef struct HashMapEntry HashMapEntry;

StringBuilder* createStringBuilder(size_t initial_capacity) {
    size_t cap = initial_capacity > 0 ? initial_capacity : VM_SMALL_STRING_BUFFER;
    StringBuilder* sb = (StringBuilder*)reallocate(NULL, 0, sizeof(StringBuilder));
    sb->capacity = cap;
    sb->buffer = (char*)reallocate(NULL, 0, sb->capacity);
    sb->length = 0;
    return sb;
}

void appendToStringBuilder(StringBuilder* sb, const char* str, size_t len) {
    if (sb->length + len + 1 > sb->capacity) {
        size_t newCap = (sb->length + len + 1) * 2;
        sb->buffer = (char*)reallocate(sb->buffer, sb->capacity, newCap);
        sb->capacity = newCap;
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
}

ObjString* stringBuilderToString(StringBuilder* sb) {
    sb->buffer[sb->length] = '\0';
    ObjString* result = intern_string(sb->buffer, (int)sb->length);
    return result;
}

void freeStringBuilder(StringBuilder* sb) {
    reallocate(sb->buffer, sb->capacity, 0);
    reallocate(sb, sizeof(StringBuilder), 0);
}

StringRope* rope_from_cstr(const char* str, size_t len) {
    StringRope* r = (StringRope*)reallocate(NULL, 0, sizeof(StringRope));
    r->kind = ROPE_LEAF;
    r->as.leaf.data = (char*)reallocate(NULL, 0, len + 1);
    memcpy(r->as.leaf.data, str, len);
    r->as.leaf.data[len] = '\0';
    r->as.leaf.len = len;
    r->as.leaf.is_ascii = true;
    r->as.leaf.is_interned = false;
    r->hash_valid = false;
    r->hash_cache = 0;
    return r;
}

static void rope_flatten(StringRope* rope, StringBuilder* sb) {
    if (!rope) return;
    switch (rope->kind) {
        case ROPE_LEAF:
            appendToStringBuilder(sb, rope->as.leaf.data, rope->as.leaf.len);
            break;
        case ROPE_CONCAT:
            rope_flatten(rope->as.concat.left, sb);
            rope_flatten(rope->as.concat.right, sb);
            break;
        case ROPE_SUBSTRING:
            appendToStringBuilder(sb,
                                  rope->as.substring.base->as.leaf.data + rope->as.substring.start,
                                  rope->as.substring.len);
            break;
    }
}

char* rope_to_cstr(StringRope* rope) {
    if (!rope) return NULL;
    StringBuilder* sb = createStringBuilder(rope->kind == ROPE_LEAF ? rope->as.leaf.len + 1 : 16);
    rope_flatten(rope, sb);
    sb->buffer[sb->length] = '\0';
    char* result = sb->buffer;
    free(sb); // only free the struct, keep buffer
    return result;
}

StringInternTable globalStringTable;

void init_string_table(StringInternTable* table) {
    table->interned = hashmap_new();
    table->threshold = 32;
    table->total_interned = 0;
}

ObjString* intern_string(const char* chars, int length) {
    size_t hash = 5381;
    for (int i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)chars[i];
    }
    char keybuf[32];
    snprintf(keybuf, sizeof(keybuf), "%zu", hash);
    ObjString* existing = (ObjString*)hashmap_get(globalStringTable.interned, keybuf);
    if (existing && existing->length == length && memcmp(existing->chars, chars, length) == 0) {
        return existing;
    }
    ObjString* s = allocateString(chars, length);
    s->rope->as.leaf.is_interned = true;
    hashmap_set(globalStringTable.interned, keybuf, s);
    globalStringTable.total_interned++;
    return s;
}


void free_rope(StringRope* rope) {
    if (!rope) return;
    switch (rope->kind) {
        case ROPE_LEAF:
            free(rope->as.leaf.data);
            break;
        case ROPE_CONCAT:
            free_rope(rope->as.concat.left);
            free_rope(rope->as.concat.right);
            break;
        case ROPE_SUBSTRING:
            free_rope(rope->as.substring.base);
            break;
    }
    free(rope);
}

// --- HashMapIterator stub implementation ---
typedef struct {
    size_t bucket_idx;
    void* current_entry;
    void* map;
} HashMapIterator;

static void hashmap_iterator_init(HashMap* map, HashMapIterator* it) {
    it->map = map;
    it->bucket_idx = 0;
    it->current_entry = NULL;
}

static int hashmap_iterator_has_next(HashMapIterator* it) {
    HashMap* map = (HashMap*)it->map;
    if (!map) return 0;
    while (it->bucket_idx < map->capacity) {
        HashMapEntry* entry = (HashMapEntry*)it->current_entry;
        if (!entry) entry = map->buckets[it->bucket_idx];
        if (entry) {
            it->current_entry = entry;
            return 1;
        }
        it->bucket_idx++;
        it->current_entry = NULL;
    }
    return 0;
}

static void* hashmap_iterator_next_value(HashMapIterator* it) {
    HashMapEntry* entry = (HashMapEntry*)it->current_entry;
    if (!entry) return NULL;
    void* value = entry->value;
    it->current_entry = entry->next;
    if (!it->current_entry) it->bucket_idx++;
    return value;
}

void free_string_table(StringInternTable* table) {
    if (!table || !table->interned) return;
    HashMapIterator it;
    hashmap_iterator_init(table->interned, &it);
    while (hashmap_iterator_has_next(&it)) {
        ObjString* s = (ObjString*)hashmap_iterator_next_value(&it);
        if (s) {
            if (s->rope) free_rope(s->rope);
            free(s->chars);
            free(s);
        }
    }
    hashmap_free(table->interned);
    table->interned = NULL;
    table->total_interned = 0;
}

