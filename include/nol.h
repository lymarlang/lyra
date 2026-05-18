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

typedef enum { NOL_NULL, NOL_BOOL, NOL_INT, NOL_FLOAT, NOL_STRING, NOL_ARRAY, NOL_OBJECT } nol_type;
struct nol_val;
typedef struct { char* k; struct nol_val* v; } nol_entry;
typedef struct { struct nol_val** e; size_t n; } nol_array;
typedef struct { nol_entry* e; size_t n; } nol_object;
typedef struct nol_val {
    nol_type type;
    union { bool b; long long i; double f; char* s; nol_array a; nol_object o; } u;
} nol_val;
typedef struct { char msg[256]; int line, col; } nol_err_t;

nol_val* nol_new_null() { return (nol_val*)calloc(1, sizeof(nol_val)); }
nol_val* nol_new_bool(bool b) { nol_val* v = nol_new_null(); v->type = NOL_BOOL; v->u.b = b; return v; }
nol_val* nol_new_int(long long i) { nol_val* v = nol_new_null(); v->type = NOL_INT; v->u.i = i; return v; }
nol_val* nol_new_float(double f) { nol_val* v = nol_new_null(); v->type = NOL_FLOAT; v->u.f = f; return v; }
nol_val* nol_new_string(const char* s) { nol_val* v = nol_new_null(); v->type = NOL_STRING; v->u.s = strdup(s?s:""); return v; }
nol_val* nol_new_array() { nol_val* v = nol_new_null(); v->type = NOL_ARRAY; return v; }
void nol_array_add(nol_val* a, nol_val* v) { if(!a || a->type != NOL_ARRAY || !v) return; a->u.a.e = (nol_val**)realloc(a->u.a.e, sizeof(nol_val*) * (a->u.a.n + 1)); a->u.a.e[a->u.a.n++] = v; }
nol_val* nol_new_object() { nol_val* v = nol_new_null(); v->type = NOL_OBJECT; return v; }
void nol_object_set(nol_val* o, const char* k, nol_val* v) {
    if (!o || o->type != NOL_OBJECT || !k) { if(v) nol_free(v); return; }
    for (size_t i=0; i<o->u.o.n; i++) if (o->u.o.e[i].k && strcmp(o->u.o.e[i].k, k) == 0) {
        nol_val* old = o->u.o.e[i].v; nol_free(old); o->u.o.e[i].v = v; return;
    }
    o->u.o.e = (nol_entry*)realloc(o->u.o.e, sizeof(nol_entry) * (o->u.o.n + 1));
    o->u.o.e[o->u.o.n].k = strdup(k); o->u.o.e[o->u.o.n++].v = v;
}
void nol_free(nol_val* v) {
    if (!v) return;
    if (v->type == NOL_STRING) free(v->u.s);
    else if (v->type == NOL_ARRAY) { for (size_t i = 0; i < v->u.a.n; i++) nol_free(v->u.a.e[i]); free(v->u.a.e); }
    else if (v->type == NOL_OBJECT) { for (size_t i = 0; i < v->u.o.n; i++) { free(v->u.o.e[i].k); nol_free(v->u.o.e[i].v); } free(v->u.o.e); }
    free(v);
}
nol_val* nol_get(const nol_val* v, const char* path) {
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

#ifdef NOL_IMPLEMENTATION
static void _set_err(nol_err_t* err, const char* m, int l, int c) { if (err) { strncpy(err->msg, m, 255); err->msg[255]=0; err->line=l; err->col=c; } }
typedef struct { const char* s; size_t p, len; int l, c; clock_t start; int depth; } _prs_t;
static char _pk(_prs_t* p) { return p->p < p->len ? p->s[p->p] : '\0'; }
static char _ad(_prs_t* p) { char ch = _pk(p); if (ch) { p->p++; if (ch == '\n') { p->l++; p->c = 1; } else p->c++; } return ch; }
static void _sk(_prs_t* p) { while (1) { char ch = _pk(p); if (isspace((unsigned char)ch)) _ad(p); else if (ch == '#') { _ad(p); if (_pk(p) == '#') { _ad(p); while (p->p < p->len-1 && !(p->s[p->p] == '#' && p->s[p->p+1] == '#')) _ad(p); if (p->p < p->len) { _ad(p); _ad(p); } } else while (_pk(p) && _pk(p) != '\n') _ad(p); } else break; } }
static nol_val* _prs_v(_prs_t* p, nol_err_t* err);
static char* _prs_k(_prs_t* p) { _sk(p); char buf[4096]; int i = 0; if (_pk(p) == '"' || _pk(p) == '\'') { char q = _ad(p); while (_pk(p) && _pk(p) != q && i<4095) { if (_pk(p) == '\\') { _ad(p); char e = _ad(p); if (e == 'n') buf[i++] = '\n'; else buf[i++] = e; } else buf[i++] = _ad(p); } if (_pk(p) == q) _ad(p); } else while ((isalnum((unsigned char)_pk(p)) || _pk(p) == '_' || _pk(p) == '-') && i<4095) buf[i++] = _ad(p); buf[i]=0; return strdup(buf); }
static void _prs_p(_prs_t* p, nol_val* o, nol_err_t* err) {
    _sk(p); if (!_pk(p)) return;
    char* k = _prs_k(p); if (strcmp(k, "_env")==0 || strcmp(k, "_interpolate")==0 || strcmp(k, "_meta")==0) _set_err(err, "Reserved", p->l, p->c);
    if (o->type == NOL_OBJECT) { for (size_t i=0; i<o->u.o.n; i++) if (strcmp(o->u.o.e[i].k, k) == 0) _set_err(err, "Duplicate key", p->l, p->c); }
    _sk(p); if (_pk(p) == ':') _ad(p);
    nol_val* v = _prs_v(p, err); nol_object_set(o, k, v); free(k);
}
static nol_val* _prs_v(_prs_t* p, nol_err_t* err) {
    _sk(p); if (++p->depth > 100) return NULL;
    char ch = _pk(p); nol_val* r = NULL;
    if (ch == '{') { _ad(p); r = nol_new_object(); while (1) { _sk(p); if (_pk(p) == '}' || !_pk(p)) break; _prs_p(p, r, err); _sk(p); if (_pk(p) == ',') _ad(p); } if (_pk(p) == '}') _ad(p); }
    else if (ch == '[') { _ad(p); r = nol_new_array(); while (1) { _sk(p); if (_pk(p) == ']' || !_pk(p)) break; nol_array_add(r, _prs_v(p, err)); _sk(p); if (_pk(p) == ',') _ad(p); } if (_pk(p) == ']') _ad(p); }
    else if (ch == '"' || ch == '\'') { char q = _ad(p); size_t cap = 1024, i=0; char* b = (char*)malloc(cap); while (_pk(p) && _pk(p) != q) { if (i+10 >= cap) { cap *= 2; b = (char*)realloc(b, cap); } if (_pk(p) == '\\') { _ad(p); char e = _ad(p); if (e == 'n') b[i++] = '\n'; else if (e == 'r') b[i++] = '\r'; else if (e == 't') b[i++] = '\t'; else if (e == 'u' || e == 'U') { int n = (e == 'u' ? 4 : 8); char hb[9]; for(int j = 0; j < n; j++) hb[j] = _ad(p); hb[n] = 0; unsigned int u = strtoul(hb, NULL, 16); if (u < 0x80) b[i++] = (char)u; else if (u < 0x800) { b[i++] = (char)(0xc0 | (u >> 6)); b[i++] = (char)(0x80 | (u & 0x3f)); } else if (u < 0x10000) { b[i++] = (char)(0xe0 | (u >> 12)); b[i++] = (char)(0x80 | ((u >> 6) & 0x3f)); b[i++] = (char)(0x80 | (u & 0x3f)); } else { b[i++] = (char)(0xf0 | (u >> 18)); b[i++] = (char)(0x80 | ((u >> 12) & 0x3f)); b[i++] = (char)(0x80 | ((u >> 6) & 0x3f)); b[i++] = (char)(0x80 | (u & 0x3f)); } } else b[i++] = e; } else b[i++] = _ad(p); } b[i]=0; if (_pk(p) == q) _ad(p); r = nol_new_string(b); free(b); }
    else if (isdigit((unsigned char)ch) || ch == '-') { char b[64]; int i = 0; while ((isdigit((unsigned char)_pk(p)) || strchr(".-eE+", _pk(p))) && i < 63) b[i++] = _ad(p); b[i] = 0; if (strchr(b, '.')) r = nol_new_float(atof(b)); else r = nol_new_int(atoll(b)); }
    else { char b[32]; int i = 0; while (isalpha((unsigned char)_pk(p)) && i < 31) b[i++] = _ad(p); b[i] = 0; if (strcmp(b, "true") == 0) r = nol_new_bool(true); else if (strcmp(b, "false") == 0) r = nol_new_bool(false); else if (strcmp(b, "null") == 0) r = nol_new_null(); }
    p->depth--; return r;
}
static void _prs_into(_prs_t* p, nol_val* o, nol_err_t* err) { while (1) { _sk(p); char c = _pk(p); if (c == '[' || !c) break; _prs_p(p, o, err); } }
nol_val* nol_parse(const char* input, nol_err_t* err) {
    if (!input) return nol_new_object(); _prs_t p = { input, 0, strlen(input), 1, 1, clock(), 0 }; nol_val* root = nol_new_object();
    while (1) {
        _sk(&p); if (!_pk(&p)) break;
        if (_pk(&p) == '[') {
            _ad(&p); char path[256]; int i = 0; while (_pk(&p) && _pk(&p) != ']' && i < 255) path[i++] = _ad(&p); path[i] = 0; if (_pk(&p) == ']') _ad(&p);
            char* start = path; nol_val* curr = root;
            while (1) {
                char* d = strchr(start, '.'); if (d) *d = 0; bool last = (d == NULL); nol_val* next = NULL;
                for (size_t j = 0; j < curr->u.o.n; j++) if (strcmp(curr->u.o.e[j].k, start) == 0) next = curr->u.o.e[j].v;
                if (!next) { next = nol_new_object(); nol_object_set(curr, start, next); }
                if (last) { _prs_into(&p, next, err); break; }
                curr = next; start = d + 1;
            }
        } else _prs_p(&p, root, err);
    }
    return root;
}
static void _ap(char** b, size_t* s, size_t* c, const char* x) { size_t l = strlen(x); while (*s + l + 1 >= *c) { *c *= 2; *b = (char*)realloc(*b, *c); } strcpy(*b + *s, x); *s += l; }
static int _cmp_ent(const void* a, const void* b) { return strcmp(((nol_entry*)a)->k, ((nol_entry*)b)->k); }
static void _dm(const nol_val* v, int ind, int lvl, bool root, char** b, size_t* s, size_t* c) {
    if (!v) { _ap(b, s, c, "null"); return; }
    switch (v->type) {
        case NOL_NULL: _ap(b, s, c, "null"); break; case NOL_BOOL: _ap(b, s, c, v->u.b ? "true" : "false"); break;
        case NOL_INT: { char buf[64]; snprintf(buf, 64, "%lld", v->u.i); _ap(b, s, c, buf); break; }
        case NOL_FLOAT: { char buf[64]; snprintf(buf, 64, "%.5f", v->u.f); char* p = buf + strlen(buf) - 1; while (p > buf && *p == '0') *p-- = 0; if (*p == '.') *p-- = 0; _ap(b, s, c, buf); break; }
        case NOL_STRING: _ap(b, s, c, "\""); _ap(b, s, c, v->u.s); _ap(b, s, c, "\""); break;
        case NOL_ARRAY: _ap(b, s, c, "["); for (size_t i = 0; i < v->u.a.n; i++) { _dm(v->u.a.e[i], ind, lvl + 1, false, b, s, c); if (i + 1 < v->u.a.n) _ap(b, s, c, ", "); } _ap(b, s, c, "]"); break;
        case NOL_OBJECT: if (v->u.o.n > 0) qsort(v->u.o.e, v->u.o.n, sizeof(nol_entry), _cmp_ent); if (!root) _ap(b, s, c, "{");
            for (size_t i = 0; i < v->u.o.n; i++) {
                if (!v->u.o.e[i].k || v->u.o.e[i].k[0] == '_') continue;
                _ap(b, s, c, "\n"); for (int j = 0; j < (lvl + 1) * ind; j++) _ap(b, s, c, " ");
                _ap(b, s, c, v->u.o.e[i].k); _ap(b, s, c, ": "); _dm(v->u.o.e[i].v, ind, lvl + 1, false, b, s, c);
                if (i + 1 < v->u.o.n) _ap(b, s, c, ",");
            }
            _ap(b, s, c, "\n"); for (int j = 0; j < lvl * ind; j++) _ap(b, s, c, " "); if (!root) _ap(b, s, c, "}"); break;
    }
}
char* nol_dump(const nol_val* v, int indent) { size_t c = 1024, s = 0; char* b = (char*)malloc(c); b[0] = 0; _dm(v, indent, 0, true, &b, &s, &c); return b; }
#endif
#ifdef __cplusplus
}
#endif
#endif
