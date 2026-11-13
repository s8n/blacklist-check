// Small JSON reader, enough for blacklist.json.
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;

typedef struct JVal {
    JType type;
    bool b;
    double num;
    char *str;           // J_STR
    struct JVal *items;  // J_ARR, J_OBJ
    char **keys;         // J_OBJ
    int n;
} JVal;

// Parses len bytes. Returns NULL on a syntax error and sets *err_pos.
JVal *json_parse(const char *text, size_t len, size_t *err_pos);
void json_free(JVal *v);

const JVal *json_get(const JVal *obj, const char *key);
// Number, or a string holding a number ("0x0100..." included). false if neither.
bool json_u64(const JVal *v, unsigned long long *out);
