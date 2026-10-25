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
