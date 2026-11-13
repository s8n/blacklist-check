#include "json.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *s;
    size_t len, pos;
    int depth;
} P;

static void ws(P *p) {
    while (p->pos < p->len && strchr(" \t\r\n", p->s[p->pos])) p->pos++;
}

static bool lit(P *p, const char *w) {
    size_t n = strlen(w);
    if (p->len - p->pos < n || memcmp(p->s + p->pos, w, n)) return false;
    p->pos += n;
    return true;
}

static bool value(P *p, JVal *out);
static void free_inner(JVal *v);

// Escapes other than \uXXXX are kept literally; \uXXXX becomes '?' unless ASCII.
static char *string(P *p) {
    if (p->pos >= p->len || p->s[p->pos] != '"') return NULL;
    p->pos++;
    char *out = malloc(p->len - p->pos + 1);
    if (!out) return NULL;
    size_t o = 0;
    while (p->pos < p->len) {
        char c = p->s[p->pos++];
        if (c == '"') { out[o] = 0; return out; }
        if (c != '\\') { out[o++] = c; continue; }
        if (p->pos >= p->len) break;
        c = p->s[p->pos++];
        switch (c) {
        case 'n': out[o++] = '\n'; break;
        case 't': out[o++] = '\t'; break;
        case 'r': out[o++] = '\r'; break;
        case 'b': out[o++] = '\b'; break;
        case 'f': out[o++] = '\f'; break;
        case 'u': {
            if (p->len - p->pos < 4) goto fail;
            char hex[5] = {0};
            memcpy(hex, p->s + p->pos, 4);
            p->pos += 4;
            long cp = strtol(hex, NULL, 16);
            out[o++] = cp > 0 && cp < 0x80 ? (char)cp : '?';
            break;
        }
        default: out[o++] = c; break;
        }
    }
fail:
    free(out);
    return NULL;
}

static bool push(JVal *v, JVal *item, char *key) {
    JVal *items = realloc(v->items, sizeof(JVal) * (v->n + 1));
    if (!items) return false;
    v->items = items;
    if (v->type == J_OBJ) {
        char **keys = realloc(v->keys, sizeof(char *) * (v->n + 1));
        if (!keys) return false;
        v->keys = keys;
        v->keys[v->n] = key;
    }
    v->items[v->n++] = *item;
    return true;
}

static bool container(P *p, JVal *out, bool obj) {
    if (++p->depth > 32) return false;
    out->type = obj ? J_OBJ : J_ARR;
    p->pos++;
    ws(p);
    if (p->pos < p->len && p->s[p->pos] == (obj ? '}' : ']')) { p->pos++; p->depth--; return true; }
    for (;;) {
        char *key = NULL;
        JVal item = {0};
        ws(p);
        if (obj) {
            if (!(key = string(p))) return false;
            ws(p);
            if (p->pos >= p->len || p->s[p->pos] != ':') { free(key); return false; }
            p->pos++;
        }
        if (!value(p, &item) || !push(out, &item, key)) {
            free(key);
            free_inner(&item);
            return false;
        }
        ws(p);
        if (p->pos >= p->len) return false;
        char c = p->s[p->pos++];
        if (c == ',') continue;
        if (c == (obj ? '}' : ']')) { p->depth--; return true; }
        return false;
    }
}

static bool value(P *p, JVal *out) {
    ws(p);
    if (p->pos >= p->len) return false;
    char c = p->s[p->pos];
    memset(out, 0, sizeof *out);
    if (c == '{') return container(p, out, true);
    if (c == '[') return container(p, out, false);
    if (c == '"') { out->type = J_STR; return (out->str = string(p)) != NULL; }
    if (lit(p, "true")) { out->type = J_BOOL; out->b = true; return true; }
    if (lit(p, "false")) { out->type = J_BOOL; return true; }
    if (lit(p, "null")) { out->type = J_NULL; return true; }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char buf[64];
        size_t n = 0;
        while (p->pos < p->len && n < sizeof buf - 1 && strchr("+-0123456789.eE", p->s[p->pos]))
            buf[n++] = p->s[p->pos++];
        buf[n] = 0;
        out->type = J_NUM;
        out->num = strtod(buf, NULL);
        return true;
    }
    return false;
}

static void free_inner(JVal *v) {
    if (v->type == J_STR) free(v->str);
    if (v->type == J_ARR || v->type == J_OBJ) {
        for (int i = 0; i < v->n; i++) {
            free_inner(&v->items[i]);
            if (v->keys) free(v->keys[i]);
        }
        free(v->items);
        free(v->keys);
    }
}

void json_free(JVal *v) {
    if (!v) return;
    free_inner(v);
    free(v);
}

JVal *json_parse(const char *text, size_t len, size_t *err_pos) {
    P p = {text, len, 0, 0};
    // Skip a UTF-8 BOM.
    if (len >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3)) p.pos = 3;
    JVal *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    if (!value(&p, v)) {
        if (err_pos) *err_pos = p.pos;
        json_free(v);
        return NULL;
    }
    return v;
}

const JVal *json_get(const JVal *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (int i = 0; i < obj->n; i++)
        if (obj->keys[i] && !strcmp(obj->keys[i], key)) return &obj->items[i];
    return NULL;
}

bool json_u64(const JVal *v, unsigned long long *out) {
    if (!v) return false;
    if (v->type == J_NUM) { *out = (unsigned long long)v->num; return true; }
    if (v->type == J_STR && v->str[0]) {
        char *end;
        *out = strtoull(v->str, &end, 0);
        return *end == 0;
    }
    return false;
}
