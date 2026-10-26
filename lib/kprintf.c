/* lib/kprintf.c - minimal printf engine */
#include "kprintf.h"
#include "console.h"
#include "serial.h"
#include "string.h"

static void emit_pad(void (*out)(char), char c, int n) { while (n-- > 0) out(c); }

static void fmt_number(void (*out)(char), unsigned long long v, bool neg, int base, bool upper,
                       int width, bool zero, bool left, const char *prefix) {
    char tmp[24];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = digits[v % (unsigned)base]; v /= (unsigned)base; }

    int plen = (int)strlen(prefix) + (neg ? 1 : 0);
    int pad = width - n - plen;
    if (!left && !zero) emit_pad(out, ' ', pad);
    if (neg) out('-');
    for (const char *p = prefix; *p; p++) out(*p);
    if (!left && zero) emit_pad(out, '0', pad);
    while (n) out(tmp[--n]);
    if (left) emit_pad(out, ' ', pad);
}

void kvprintf_cb(void (*out)(char), const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { out(*fmt); continue; }
        fmt++;

        bool zero = false, left = false, alt = false;
        for (;; fmt++) {
            if (*fmt == '0') zero = true;
            else if (*fmt == '-') left = true;
            else if (*fmt == '#') alt = true;
            else break;
        }
        int width = 0;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        int lng = 0;                                   /* 0 = int, 1 = long, 2 = long long / size_t */
        while (*fmt == 'l' || *fmt == 'z') { lng++; fmt++; if (*(fmt - 1) == 'z') lng = 2; }

        switch (*fmt) {
        case 'c': out((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int len = (int)strlen(s);
            if (!left) emit_pad(out, ' ', width - len);
            while (*s) out(*s++);
            if (left) emit_pad(out, ' ', width - len);
            break;
        }
        case 'd': case 'i': {
            long long v = lng >= 2 ? va_arg(ap, long long) : lng == 1 ? va_arg(ap, long) : va_arg(ap, int);
            bool neg = v < 0;
            fmt_number(out, neg ? 0ULL - (unsigned long long)v : (unsigned long long)v, neg, 10, false, width, zero, left, "");
            break;
        }
        case 'u': case 'x': case 'X': {
            unsigned long long v = lng >= 2 ? va_arg(ap, unsigned long long)
                                 : lng == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            int base = *fmt == 'u' ? 10 : 16;
            fmt_number(out, v, false, base, *fmt == 'X', width, zero, left, (alt && base == 16 && v) ? "0x" : "");
            break;
        }
        case 'p':
            fmt_number(out, (unsigned long long)(uintptr_t)va_arg(ap, void *), false, 16, false, width, zero, left, "0x");
            break;
        case '%': out('%'); break;
        case 0: return;
        default: out('%'); out(*fmt); break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    kvprintf_cb(console_putc, fmt, ap);
    va_end(ap);
}

void klog(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    kvprintf_cb(serial_putc, fmt, ap);
    va_end(ap);
}
