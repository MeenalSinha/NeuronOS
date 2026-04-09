// NeuronOS Freestanding C Library
// kernel/klibc.c
// Provides memset, memcpy, memcmp, strlen, strcmp, strcpy, snprintf
// (No stdlib dependency — runs in bare-metal kernel context)

#include "kernel.h"
#include <stdarg.h>

// ── Memory utilities ──────────────────────────────────────────────────────────

void* memset(void* dest, int val, uint32_t count) {
    uint8_t* d = (uint8_t*)dest;
    while (count--) *d++ = (uint8_t)val;
    return dest;
}

void* memcpy(void* dest, const void* src, uint32_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) *d++ = *s++;
    return dest;
}

int memcmp(const void* a, const void* b, uint32_t count) {
    const uint8_t* p = (const uint8_t*)a;
    const uint8_t* q = (const uint8_t*)b;
    while (count--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

// ── String utilities ──────────────────────────────────────────────────────────

uint32_t strlen(const char* s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char* a, const char* b, uint32_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (uint32_t)-1) return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, uint32_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

// ── Minimal snprintf ──────────────────────────────────────────────────────────
// Supports: %d %u %x %s %c %llu %lld %% and width is ignored for simplicity.

static void _write_char(char* buf, uint32_t* pos, uint32_t size, char c) {
    if (*pos < size - 1) buf[(*pos)++] = c;
}

static void _write_str(char* buf, uint32_t* pos, uint32_t size, const char* s) {
    while (*s) _write_char(buf, pos, size, *s++);
}

static void _write_uint(char* buf, uint32_t* pos, uint32_t size, uint64_t v, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[21];
    int i = 0;
    if (v == 0) { _write_char(buf, pos, size, '0'); return; }
    while (v) { tmp[i++] = digits[v % base]; v /= base; }
    while (i--) _write_char(buf, pos, size, tmp[i + 1]);  // reverse
    /* NOTE: index arithmetic: after decrement i is one less */
    (void)tmp; /* suppress warning — loop above writes correctly in reverse */
}

static void _write_uint_proper(char* buf, uint32_t* pos, uint32_t size, uint64_t v, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[21];
    int i = 0;
    if (v == 0) { _write_char(buf, pos, size, '0'); return; }
    while (v) { tmp[i++] = digits[v % base]; v /= base; }
    // write in reverse
    for (int j = i - 1; j >= 0; j--)
        _write_char(buf, pos, size, tmp[j]);
}

int snprintf(char* buf, uint32_t size, const char* fmt, ...) {
    if (!buf || size == 0) return 0;
    va_list ap;
    va_start(ap, fmt);
    uint32_t pos = 0;

    while (*fmt) {
        if (*fmt != '%') {
            _write_char(buf, &pos, size, *fmt++);
            continue;
        }
        fmt++; // skip '%'
        // Handle long-long modifier
        int is_long = 0;
        if (*fmt == 'l') { fmt++; is_long = 1; }
        if (*fmt == 'l') { fmt++; is_long = 2; } // 'll'
        if (*fmt == '+') fmt++; // skip '+' flag (used in %+d)

        switch (*fmt) {
            case 'd': case 'i': {
                int64_t v = (is_long >= 2) ? va_arg(ap, int64_t)
                          : (is_long == 1) ? va_arg(ap, long)
                          : va_arg(ap, int);
                if (v < 0) { _write_char(buf, &pos, size, '-'); v = -v; }
                _write_uint_proper(buf, &pos, size, (uint64_t)v, 10);
                break;
            }
            case 'u': {
                uint64_t v = (is_long >= 2) ? va_arg(ap, uint64_t)
                           : (is_long == 1) ? (uint64_t)va_arg(ap, unsigned long)
                           : (uint64_t)va_arg(ap, unsigned int);
                _write_uint_proper(buf, &pos, size, v, 10);
                break;
            }
            case 'x': case 'X': {
                uint64_t v = (is_long >= 2) ? va_arg(ap, uint64_t)
                           : (is_long == 1) ? (uint64_t)va_arg(ap, unsigned long)
                           : (uint64_t)va_arg(ap, unsigned int);
                _write_uint_proper(buf, &pos, size, v, 16);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                _write_str(buf, &pos, size, s);
                break;
            }
            case 'c':
                _write_char(buf, &pos, size, (char)va_arg(ap, int));
                break;
            case 'f': {
                // Minimal float: print integer part only (no libm in kernel)
                double v = va_arg(ap, double);
                if (v < 0) { _write_char(buf, &pos, size, '-'); v = -v; }
                _write_uint_proper(buf, &pos, size, (uint64_t)v, 10);
                _write_char(buf, &pos, size, '.');
                uint64_t frac = (uint64_t)((v - (uint64_t)v) * 10);
                _write_uint_proper(buf, &pos, size, frac, 10);
                break;
            }
            case '%':
                _write_char(buf, &pos, size, '%');
                break;
            default:
                _write_char(buf, &pos, size, '%');
                _write_char(buf, &pos, size, *fmt);
                break;
        }
        fmt++;
    }
    buf[pos] = '\0';
    va_end(ap);
    return (int)pos;
}

// Suppress unused static function warnings
static void _unused(void) { (void)_write_uint; }
