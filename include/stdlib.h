#pragma once
#include <stdint.h>
#include <string.h>

extern void * realloc(void * ptr, size_t size);
extern void free(void *ptr);
extern void * malloc(size_t size);
extern void * calloc(size_t nmemb, size_t size);
extern long int strtol(const char *nptr, char **endptr, int base);
extern long long int strtoll(const char *nptr, char **endptr, int base);
extern unsigned long int strtoul(const char *nptr, char **endptr, int base);
extern void abort(void);
extern void exit(int status);
extern int atoi(const char * c);
