#include "../../include/kernel/stdio.h"
#include "../../include/drivers/serial.h"
#include "../../include/drivers/vga.h"
#include "../../include/kernel/types.h"
#include <stdarg.h>

static int itoa_hex(char *buf, uint64_t v, int upper) {
  const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  char tmp[32];
  int i = 0;
  if (v == 0) {
    buf[0] = '0';
    buf[1] = 0;
    return 1;
  }
  while (v) {
    tmp[i++] = d[v & 0xF];
    v >>= 4;
  }
  int n = 0;
  while (i) {
    buf[n++] = tmp[--i];
  }
  buf[n] = 0;
  return n;
}
static int itoa_dec(char *buf, int64_t v, int uns) {
  char tmp[32];
  int i = 0;
  int neg = 0;
  uint64_t x;
  if (!uns && v < 0) {
    neg = 1;
    x = (uint64_t)(-v);
  } else
    x = (uint64_t)v;
  if (x == 0) {
    buf[0] = '0';
    buf[1] = 0;
    return 1;
  }
  while (x) {
    tmp[i++] = (char)('0' + (x % 10));
    x /= 10;
  }
  int n = 0;
  if (neg)
    buf[n++] = '-';
  while (i) {
    buf[n++] = tmp[--i];
  }
  buf[n] = 0;
  return n;
}
static int kvsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap) {
  size_t pos = 0;
  for (size_t i = 0; fmt[i]; i++) {
    if (fmt[i] != '%') {
      if (out && pos + 1 < out_sz)
        out[pos] = fmt[i];
      pos++;
      continue;
    }
    i++;
    char c = fmt[i];
    if (c == '%') {
      if (out && pos + 1 < out_sz)
        out[pos] = '%';
      pos++;
      continue;
    }
    if (c == 's') {
      const char *s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      for (size_t j = 0; s[j]; j++) {
        if (out && pos + 1 < out_sz)
          out[pos] = s[j];
        pos++;
      }
      continue;
    }
    if (c == 'c') {
      char ch = (char)va_arg(ap, int);
      if (out && pos + 1 < out_sz)
        out[pos] = ch;
      pos++;
      continue;
    }
    if (c == 'd') {
      char buf[64];
      int n = itoa_dec(buf, va_arg(ap, int), 0);
      for (int j = 0; j < n; j++) {
        if (out && pos + 1 < out_sz)
          out[pos] = buf[j];
        pos++;
      }
      continue;
    }
    if (c == 'u') {
      char buf[64];
      int n = itoa_dec(buf, (int64_t)va_arg(ap, unsigned int), 1);
      for (int j = 0; j < n; j++) {
        if (out && pos + 1 < out_sz)
          out[pos] = buf[j];
        pos++;
      }
      continue;
    }
    if (c == 'x' || c == 'X') {
      char buf[64];
      int n = itoa_hex(buf, (uint64_t)va_arg(ap, unsigned long long), c == 'X');
      for (int j = 0; j < n; j++) {
        if (out && pos + 1 < out_sz)
          out[pos] = buf[j];
        pos++;
      }
      continue;
    }
    if (c == 'p') {
      char buf[64];
      if (out && pos + 1 < out_sz)
        out[pos] = '0';
      pos++;
      if (out && pos + 1 < out_sz)
        out[pos] = 'x';
      pos++;
      int n = itoa_hex(buf, (uint64_t)va_arg(ap, void *), 0);
      for (int j = 0; j < n; j++) {
        if (out && pos + 1 < out_sz)
          out[pos] = buf[j];
        pos++;
      }
      continue;
    }
  }
  if (out && out_sz)
    out[(pos < out_sz - 1) ? pos : (out_sz - 1)] = 0;
  return (int)pos;
}
int ksprintf(char *buffer, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = kvsnprintf(buffer, (size_t)-1, format, ap);
  va_end(ap);
  return n;
}
int kprintf(const char *format, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, format);
  int n = kvsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  buf[(n < 1023) ? n : 1023] = 0;
  vga_write(buf);
  serial_write_string(buf);
  return n;
}
void kputs(const char *str) {
  if (!str)
    return;
  vga_write(str);
  serial_write_string(str);
}
