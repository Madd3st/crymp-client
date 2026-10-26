// Linux syntax-check shim for MSVC-only CryEngine headers (WarsBots offline compile check). Never shipped.
#pragma once
#include <strings.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
static inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
static inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }
static inline int _wcsicmp(const wchar_t* a, const wchar_t* b) { return wcscasecmp(a, b); }
static inline int _wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n) { return wcsncasecmp(a, b, n); }
#define __min(a,b) (((a)<(b))?(a):(b))
#define __max(a,b) (((a)>(b))?(a):(b))
#include <memory>
#include <math.h>
#include <stdio.h>
template <size_t N> inline int strcpy_s(char (&d)[N], const char* s) { strncpy(d, s, N - 1); d[N - 1] = 0; return 0; }
inline int strcpy_s(char* d, size_t n, const char* s) { strncpy(d, s, n - 1); d[n - 1] = 0; return 0; }
#define _isnan isnan
#define _MAX_PATH 260
#define _InterlockedIncrement(p) __sync_add_and_fetch((p), 1)
#define _InterlockedDecrement(p) __sync_sub_and_fetch((p), 1)
#define _InterlockedExchangeAdd(p, v) __sync_fetch_and_add((p), (v))
#define _InterlockedCompareExchange(p, x, c) __sync_val_compare_and_swap((p), (c), (x))
#define _InterlockedExchange(p, v) __sync_lock_test_and_set((p), (v))
#define _InterlockedOr(p, v) __sync_fetch_and_or((p), (v))
#define _InterlockedAnd(p, v) __sync_fetch_and_and((p), (v))
static inline wchar_t* _wgetenv(const wchar_t*) { return nullptr; }
#include <locale.h>
#include <stdarg.h>
typedef locale_t _locale_t;
static inline _locale_t _create_locale(int, const char* n) { return newlocale(LC_NUMERIC_MASK, n, (locale_t)0); }
static inline void _free_locale(_locale_t l) { freelocale(l); }
static inline FILE* _wfopen(const wchar_t*, const wchar_t*) { return nullptr; }
static inline int _fprintf_l(FILE* f, const char* fmt, _locale_t, ...) { va_list a; va_start(a, fmt); int r = vfprintf(f, fmt, a); va_end(a); return r; }
static inline FILE* _wfopen(const char*, const wchar_t*) { return nullptr; }
#include <time.h>
#define _snprintf snprintf
#define _vsnprintf vsnprintf
static inline int localtime_s(struct tm* out, const time_t* t) { return localtime_r(t, out) ? 0 : 1; }
