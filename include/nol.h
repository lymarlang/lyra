#ifndef NOL_H
#define NOL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NOL_CORE_TYPES
#define NOL_CORE_TYPES
typedef enum { NOL_NULL, NOL_BOOL, NOL_INT, NOL_FLOAT, NOL_STRING, NOL_ARRAY, NOL_OBJECT } nol_type;
struct nol_val;
typedef struct { char* k; struct nol_val* v; } nol_entry;
typedef struct { struct nol_val** e; size_t n; } nol_array;
typedef struct { nol_entry* e; size_t n; } nol_object;
typedef struct nol_val {
    nol_type type;
    union { bool b; long long i; double f; char* s; nol_array a; nol_object o; } u;
} nol_val;
typedef struct { nol_val* root; } nol_doc;
typedef struct { nol_val* root; } nol_builder;
typedef struct { char msg[256]; int line, col; } nol_err_t;

static inline void nol_free(nol_val* v);
static inline nol_val* nol_new_null() { return (nol_val*)calloc(1, sizeof(nol_val)); }
static inline nol_val* nol_new_bool(bool b) { nol_val* v = nol_new_null(); v->type = NOL_BOOL; v->u.b = b; return v; }
static inline nol_val* nol_new_int(long long i) { nol_val* v = nol_new_null(); v->type = NOL_INT; v->u.i = i; return v; }
static inline nol_val* nol_new_float(double f) { nol_val* v = nol_new_null(); v->type = NOL_FLOAT; v->u.f = f; return v; }
static inline nol_val* nol_new_string(const char* s) { nol_val* v = nol_new_null(); v->type = NOL_STRING; v->u.s = strdup(s?s:""); return v; }
static inline nol_val* nol_new_array() { nol_val* v = nol_new_null(); v->type = NOL_ARRAY; return v; }
static inline void nol_array_add(nol_val* a, nol_val* v) { if(!a || a->type != NOL_ARRAY || !v) return; a->u.a.e = (nol_val**)realloc(a->u.a.e, sizeof(nol_val*) * (a->u.a.n + 1)); a->u.a.e[a->u.a.n++] = v; }
static inline nol_val* nol_new_object() { nol_val* v = nol_new_null(); v->type = NOL_OBJECT; return v; }
static inline void nol_object_set(nol_val* o, const char* k, nol_val* v) {
    if (!o || o->type != NOL_OBJECT || !k) { if(v) nol_free(v); return; }
    for (size_t i=0; i<o->u.o.n; i++) if (o->u.o.e[i].k && strcmp(o->u.o.e[i].k, k) == 0) {
        nol_val* old = o->u.o.e[i].v; nol_free(old); o->u.o.e[i].v = v; return;
    }
    o->u.o.e = (nol_entry*)realloc(o->u.o.e, sizeof(nol_entry) * (o->u.o.n + 1));
    o->u.o.e[o->u.o.n].k = strdup(k); o->u.o.e[o->u.o.n++].v = v;
}
static inline void nol_free(nol_val* v) {
    if (!v) return;
    if (v->type == NOL_STRING) free(v->u.s);
    else if (v->type == NOL_ARRAY) { for (size_t i = 0; i < v->u.a.n; i++) nol_free(v->u.a.e[i]); free(v->u.a.e); }
    else if (v->type == NOL_OBJECT) { for (size_t i = 0; i < v->u.o.n; i++) { free(v->u.o.e[i].k); nol_free(v->u.o.e[i].v); } free(v->u.o.e); }
    free(v);
}
static inline nol_val* nol_get(const nol_val* v, const char* path) {
    if (!v || v->type != NOL_OBJECT || !path) return NULL;
    char p[512]; strncpy(p, path, 511); p[511]=0;
    char* start = p; nol_val* curr = (nol_val*)v;
    while (start && *start) {
        char* dot = strchr(start, '.'); if (dot) *dot = 0;
        bool found = false; if (curr->type == NOL_OBJECT) {
            for (size_t i = 0; i < curr->u.o.n; i++) if (curr->u.o.e[i].k && strcmp(curr->u.o.e[i].k, start) == 0) { curr = curr->u.o.e[i].v; found = true; break; }
        }
        if (!found) return NULL;
        start = dot ? dot+1 : NULL;
    }
    return curr;
}
static inline nol_doc* nol_doc_new(nol_val* root) { nol_doc* d = (nol_doc*)malloc(sizeof(nol_doc)); d->root = root; return d; }
static inline void nol_doc_free(nol_doc* d) { if(!d) return; nol_free(d->root); free(d); }
static inline nol_val* nol_doc_get(const nol_doc* d, const char* path) { return nol_get(d->root, path); }
static inline bool nol_doc_exists(const nol_doc* d, const char* path) { return nol_get(d->root, path) != NULL; }
static inline nol_builder* nol_builder_new() { nol_builder* b = (nol_builder*)malloc(sizeof(nol_builder)); b->root = nol_new_object(); return b; }
static inline void nol_builder_free(nol_builder* b) { if(!b) return; nol_free(b->root); free(b); }
static inline void nol_builder_set(nol_builder* b, const char* path, nol_val* val) {
    if (!b || !path) return;
    char p[512]; strncpy(p, path, 511); p[511]=0;
    char* start = p; nol_val* curr = b->root;
    while (1) {
        char* dot = strchr(start, '.'); if (dot) *dot = 0;
        if (dot) {
            nol_val* next = NULL;
            for (size_t i=0; i<curr->u.o.n; i++) if (strcmp(curr->u.o.e[i].k, start) == 0) next = curr->u.o.e[i].v;
            if (!next || next->type != NOL_OBJECT) { next = nol_new_object(); nol_object_set(curr, start, next); }
            curr = next; start = dot + 1;
        } else {
            nol_object_set(curr, start, val); break;
        }
    }
}
static inline nol_doc* nol_builder_build(nol_builder* b) { nol_doc* d = nol_doc_new(b->root); b->root = NULL; nol_builder_free(b); return d; }
#endif

nol_doc* nol_parse(const char* input, nol_err_t* err);
char* nol_dump(const nol_val* v, int indent);

#ifdef NOL_IMPLEMENTATION
static void _nol_set_err(nol_err_t* err, const char* m, int l, int c) { if (err) { strncpy(err->msg, m, 255); err->msg[255]=0; err->line=l; err->col=c; } }
typedef struct { const char* s; size_t p, len; int l, c; clock_t start; int depth; } _nol_prs_t;
static char _nol_pk(_nol_prs_t* p) { return p->p < p->len ? p->s[p->p] : '\0'; }
static char _nol_ad(_nol_prs_t* p) { char ch = _nol_pk(p); if (ch) { p->p++; if (ch == '\n') { p->l++; p->c = 1; } else p->c++; } return ch; }
static void _nol_sk(_nol_prs_t* p) { while (1) { char ch = _nol_pk(p); if (isspace((unsigned char)ch)) _nol_ad(p); else if (ch == '#') { _nol_ad(p); if (_nol_pk(p) == '#') { _nol_ad(p); while (p->p < p->len-1 && !(p->s[p->p] == '#' && p->s[p->p+1] == '#')) _nol_ad(p); if (p->p < p->len) { _nol_ad(p); _nol_ad(p); } } else while (_nol_pk(p) && _nol_pk(p) != '\n') _nol_ad(p); } else break; } }
static nol_val* _nol_prs_v(_nol_prs_t* p, nol_err_t* err);
static char* _nol_prs_k(_nol_prs_t* p) { _nol_sk(p); char buf[4096]; int i = 0; if (_nol_pk(p) == '"' || _nol_pk(p) == '\'') { char q = _nol_ad(p); while (_nol_pk(p) && _nol_pk(p) != q && i<4095) { if (_nol_pk(p) == '\\') { _nol_ad(p); char e = _nol_ad(p); if (e == 'n') buf[i++] = '\n'; else buf[i++] = e; } else buf[i++] = _nol_ad(p); } if (_nol_pk(p) == q) _nol_ad(p); } else while ((isalnum((unsigned char)_nol_pk(p)) || _nol_pk(p) == '_' || _nol_pk(p) == '-') && i<4095) buf[i++] = _nol_ad(p); buf[i]=0; return strdup(buf); }
static void _nol_prs_p(_nol_prs_t* p, nol_val* o, nol_err_t* err) {
    _nol_sk(p); if (!_nol_pk(p)) return;
    char* k = _nol_prs_k(p); if (strcmp(k, "_env")==0 || strcmp(k, "_interpolate")==0 || strcmp(k, "_meta")==0) _nol_set_err(err, "Reserved", p->l, p->c);
    if (o->type == NOL_OBJECT) { for (size_t i=0; i<o->u.o.n; i++) if (strcmp(o->u.o.e[i].k, k) == 0) _nol_set_err(err, "Duplicate key", p->l, p->c); }
    _nol_sk(p); if (_nol_ad(p) == ':') ;
    nol_val* v = _nol_prs_v(p, err); nol_object_set(o, k, v); free(k);
}
static nol_val* _nol_prs_v(_nol_prs_t* p, nol_err_t* err) {
    _nol_sk(p); if (++p->depth > 100) return NULL;
    char ch = _nol_pk(p); nol_val* r = NULL;
    if (ch == '{') { _nol_ad(p); r = nol_new_object(); while (1) { _nol_sk(p); if (_nol_pk(p) == '}' || !_nol_pk(p)) break; _nol_prs_p(p, r, err); _nol_sk(p); if (_nol_pk(p) == ',') _nol_ad(p); } if (_nol_pk(p) == '}') _nol_ad(p); }
    else if (ch == '[') { _nol_ad(p); r = nol_new_array(); while (1) { _nol_sk(p); if (_nol_pk(p) == ']' || !_nol_pk(p)) break; nol_array_add(r, _nol_prs_v(p, err)); _nol_sk(p); if (_nol_pk(p) == ',') _nol_ad(p); } if (_nol_pk(p) == ']') _nol_ad(p); }
    else if (ch == '"' || ch == '\'') { char q = _nol_ad(p); size_t cap = 1024, i=0; char* b = (char*)malloc(cap); while (_nol_pk(p) && _nol_pk(p) != q) { if (i+10 >= cap) { cap *= 2; b = (char*)realloc(b, cap); } if (_nol_pk(p) == '\\') { _nol_ad(p); char e = _nol_ad(p); if (e == 'n') b[i++] = '\n'; else if (e == 'r') b[i++] = '\r'; else if (e == 't') b[i++] = '\t'; else if (e == 'u' || e == 'U') { int n = (e == 'u' ? 4 : 8); char hb[9]; for(int j = 0; j < n; j++) hb[j] = _nol_ad(p); hb[n] = 0; unsigned int u = strtoul(hb, NULL, 16); if (u < 0x80) b[i++] = (char)u; else if (u < 0x800) { b[i++] = (char)(0xc0 | (u >> 6)); b[i++] = (char)(0x80 | (u & 0x3f)); } else if (u < 0x10000) { b[i++] = (char)(0xe0 | (u >> 12)); b[i++] = (char)(0x80 | ((u >> 6) & 0x3f)); b[i++] = (char)(0x80 | (u & 0x3f)); } else { b[i++] = (char)(0xf0 | (u >> 18)); b[i++] = (char)(0x80 | ((u >> 12) & 0x3f)); b[i++] = (char)(0x80 | ((u >> 6) & 0x3f)); b[i++] = (char)(0x80 | (u & 0x3f)); } } else b[i++] = e; } else b[i++] = _nol_ad(p); } b[i]=0; if (_nol_pk(p) == q) _nol_ad(p); r = nol_new_string(b); free(b); }
    else if (isdigit((unsigned char)ch) || ch == '-') { char b[64]; int i = 0; while ((isdigit((unsigned char)_nol_pk(p)) || strchr(".-eE+", _nol_pk(p))) && i < 63) b[i++] = _nol_ad(p); b[i] = 0; if (strchr(b, '.')) r = nol_new_float(atof(b)); else r = nol_new_int(atoll(b)); }
    else { char b[32]; int i = 0; while (isalpha((unsigned char)_nol_pk(p)) && i < 31) b[i++] = _nol_ad(p); b[i] = 0; if (strcmp(b, "true") == 0) r = nol_new_bool(true); else if (strcmp(b, "false") == 0) r = nol_new_bool(false); else if (strcmp(b, "null") == 0) r = nol_new_null(); }
    p->depth--; return r;
}
static void _nol_prs_into(_nol_prs_t* p, nol_val* o, nol_err_t* err) { while (1) { _nol_sk(p); char c = _nol_pk(p); if (c == '[' || !c) break; _nol_prs_p(p, o, err); } }
nol_doc* nol_parse(const char* input, nol_err_t* err) {
    if (!input) return nol_doc_new(nol_new_object()); _nol_prs_t p = { input, 0, strlen(input), 1, 1, clock(), 0 }; nol_val* root = nol_new_object();
    while (1) {
        _nol_sk(&p); if (!_nol_pk(&p)) break;
        if (_nol_pk(&p) == '[') {
            _nol_ad(&p); char path[256]; int i = 0; while (_nol_pk(&p) && _nol_pk(&p) != ']' && i < 255) path[i++] = _nol_ad(&p); path[i] = 0; if (_nol_pk(&p) == ']') _nol_ad(&p);
            char* start = path; nol_val* curr = root;
            while (1) {
                char* d = strchr(start, '.'); if (d) *d = 0; bool last = (d == NULL); nol_val* next = NULL;
                for (size_t j = 0; j < curr->u.o.n; j++) if (strcmp(curr->u.o.e[j].k, start) == 0) next = curr->u.o.e[j].v;
                if (!next || next->type != NOL_OBJECT) { next = nol_new_object(); nol_object_set(curr, start, next); }
                if (last) { _nol_prs_into(&p, next, err); break; }
                curr = next; start = d + 1;
            }
        } else _nol_prs_p(&p, root, err);
    }
    return nol_doc_new(root);
}
static void _nol_ap(char** b, size_t* s, size_t* c, const char* x) { size_t l = strlen(x); while (*s + l + 1 >= *c) { *c *= 2; *b = (char*)realloc(*b, *c); } strcpy(*b + *s, x); *s += l; }
static int _nol_cmp_ent(const void* a, const void* b) { return strcmp(((nol_entry*)a)->k, ((nol_entry*)b)->k); }
static void _nol_dm(const nol_val* v, int ind, int lvl, bool root, char** b, size_t* s, size_t* c) {
    if (!v) { _nol_ap(b, s, c, "null"); return; }
    switch (v->type) {
        case NOL_NULL: _nol_ap(b, s, c, "null"); break; case NOL_BOOL: _nol_ap(b, s, c, v->u.b ? "true" : "false"); break;
        case NOL_INT: { char buf[64]; snprintf(buf, 64, "%lld", v->u.i); _nol_ap(b, s, c, buf); break; }
        case NOL_FLOAT: { char buf[64]; snprintf(buf, 64, "%.5f", v->u.f); char* p = buf + strlen(buf) - 1; while (p > buf && *p == '0') *p-- = 0; if (*p == '.') *p-- = 0; _nol_ap(b, s, c, buf); break; }
        case NOL_STRING: _nol_ap(b, s, c, "\""); _nol_ap(b, s, c, v->u.s); _nol_ap(b, s, c, "\""); break;
        case NOL_ARRAY: _nol_ap(b, s, c, "["); for (size_t i = 0; i < v->u.a.n; i++) { _nol_dm(v->u.a.e[i], ind, lvl + 1, false, b, s, c); if (i + 1 < v->u.a.n) _nol_ap(b, s, c, ", "); } _nol_ap(b, s, c, "]"); break;
        case NOL_OBJECT: if (v->u.o.n > 0) qsort(v->u.o.e, v->u.o.n, sizeof(nol_entry), _nol_cmp_ent); if (!root) _nol_ap(b, s, c, "{");
            for (size_t i = 0; i < v->u.o.n; i++) {
                if (!v->u.o.e[i].k || v->u.o.e[i].k[0] == '_') continue;
                _nol_ap(b, s, c, "\n"); for (int j = 0; j < (lvl + 1) * ind; j++) _nol_ap(b, s, c, " ");
                _nol_ap(b, s, c, v->u.o.e[i].k); _nol_ap(b, s, c, ": "); _nol_dm(v->u.o.e[i].v, ind, lvl + 1, false, b, s, c);
                if (i + 1 < v->u.o.n) _nol_ap(b, s, c, ",");
            }
            _nol_ap(b, s, c, "\n"); for (int j = 0; j < lvl * ind; j++) _nol_ap(b, s, c, " "); if (!root) _nol_ap(b, s, c, "}"); break;
    }
}
char* nol_dump(const nol_val* v, int indent) { size_t c = 1024, s = 0; char* b = (char*)malloc(c); b[0] = 0; _nol_dm(v, indent, 0, true, &b, &s, &c); return b; }
#endif

#ifdef __cplusplus
}
#endif
#endif
