#include "../../include/kernel/types.h"
void *memset(void *dest, int c, size_t n) {
  unsigned char *d = dest;
  for (size_t i = 0; i < n; i++)
    d[i] = (unsigned char)c;
  return dest;
}
void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  for (size_t i = 0; i < n; i++)
    d[i] = s[i];
  return dest;
}
void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  if (d < s) {
    for (size_t i = 0; i < n; i++)
      d[i] = s[i];
  } else if (d > s) {
    for (size_t i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  }
  return dest;
}
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *a = s1;
  const unsigned char *b = s2;
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i])
      return a[i] - b[i];
  }
  return 0;
}
size_t strlen(const char *str) {
  size_t l = 0;
  while (str && str[l])
    l++;
  return l;
}
char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}
char *strncpy(char *dest, const char *src, size_t n) {
  size_t i = 0;
  for (; i < n && src[i]; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = '\0';
  return dest;
}
int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}
int strncmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i] || !s1[i] || !s2[i])
      return (unsigned char)s1[i] - (unsigned char)s2[i];
  }
  return 0;
}
char *strcat(char *dest, const char *src) {
  char *d = dest;
  while (*d)
    d++;
  while ((*d++ = *src++))
    ;
  return dest;
}
