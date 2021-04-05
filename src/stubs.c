/**
 * @file stubs.c
 * @brief Implementations of libc functions needed under EFI and BIOS.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern void print_(char * str);

void abort(void) {print_("ABORT\n"); while(1); }
void exit(int status) {print_("EXIT\n"); while(1); }

int memcmp(const void * vl, const void * vr, size_t n) {
	const unsigned char *l = vl;
	const unsigned char *r = vr;
	for (; n && *l == *r; n--, l++, r++);
	return n ? *l-*r : 0;
}

void * memmove(void * dest, const void * src, size_t n) {
	char * d = dest;
	const char * s = src;

	if (d==s) {
		return d;
	}

	if (s+n <= d || d+n <= s) {
		return memcpy(d, s, n);
	}

	if (d<s) {
		if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
			while ((uintptr_t)d % sizeof(size_t)) {
				if (!n--) {
					return dest;
				}
				*d++ = *s++;
			}
			for (; n >= sizeof(size_t); n -= sizeof(size_t), d += sizeof(size_t), s += sizeof(size_t)) {
				*(size_t *)d = *(size_t *)s;
			}
		}
		for (; n; n--) {
			*d++ = *s++;
		}
	} else {
		if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
			while ((uintptr_t)(d+n) % sizeof(size_t)) {
				if (!n--) {
					return dest;
				}
				d[n] = s[n];
			}
			while (n >= sizeof(size_t)) {
				n -= sizeof(size_t);
				*(size_t *)(d+n) = *(size_t *)(s+n);
			}
		}
		while (n) {
			n--;
			d[n] = s[n];
		}
	}

	return dest;
}

int strcmp(const char * l, const char * r) {
	for (; *l == *r && *l; l++, r++);
	return *(unsigned char *)l - *(unsigned char *)r;
}

char * strchr(const char * s, int c) {
	while (*s && *s != c) s++;
	return *s == c ? (char*)s : NULL;
}

char * strrchr(const char * s, int c) {
	size_t l = strlen(s);
	for (size_t i = 0; i < l; ++i) {
		if (s[l-i-1] == c) {
			return (char*)&s[l-i-1];
		}
	}
	return NULL;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	if (!nmemb) return;
	if (!size) return;
	for (size_t i = 0; i < nmemb-1; ++i) {
		for (size_t j = 0; j < nmemb-1; ++j) {
			void * left = (char *)base + size * j;
			void * right = (char *)base + size * (j + 1);
			if (compar(left,right) > 0) {
				char tmp[size];
				memcpy(tmp, right, size);
				memcpy(right, left, size);
				memcpy(left, tmp, size);
			}
		}
	}
}

static int isspace(int c) {
	return c == ' ';
}

static int is_valid(int base, char c) {
	if (c < '0') return 0;
	if (base <= 10) {
		return c < ('0' + base);
	}

	if (c >= 'a' && c < 'a' + (base - 10)) return 1;
	if (c >= 'A' && c < 'A' + (base - 10)) return 1;
	if (c >= '0' && c <= '9') return 1;
	return 0;
}

static int convert_digit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 0xa;
	}
	if (c >= 'A' && c <= 'Z') {
		return c - 'A' + 0xa;
	}
	return 0;
}

#define LONG_MAX 2147483647

#define strtox(max, type) \
	if (base < 0 || base == 1 || base > 36) { \
		return max; \
	} \
	while (*nptr && isspace(*nptr)) nptr++; \
	int sign = 1; \
	if (*nptr == '-') { \
		sign = -1; \
		nptr++; \
	} else if (*nptr == '+') { \
		nptr++; \
	} \
	if (base == 16) { \
		if (*nptr == '0') { \
			nptr++; \
			if (*nptr == 'x') { \
				nptr++; \
			} \
		} \
	} \
	if (base == 0) { \
		if (*nptr == '0') { \
			base = 8; \
			nptr++; \
			if (*nptr == 'x') { \
				base = 16; \
				nptr++; \
			} \
		} else { \
			base = 10; \
		} \
	} \
	type val = 0; \
	while (is_valid(base, *nptr)) { \
		val *= base; \
		val += convert_digit(*nptr); \
		nptr++; \
	} \
	if (endptr) { \
		*endptr = (char *)nptr; \
	} \
	if (sign == -1) { \
		return -val; \
	} else { \
		return val; \
	}

long int strtol(const char *nptr, char **endptr, int base) {
	strtox(LONG_MAX, unsigned long int);
}

int atoi(const char * c) {
	return strtol(c,NULL,10);
}

char * strcpy(char * restrict dest, const char * restrict src) {
	char * out = dest;
	for (; (*dest=*src); src++, dest++);
	return out;
}

double strtod(const char *nptr, char **endptr) {
	int sign = 1;
	if (*nptr == '-') {
		sign = -1;
		nptr++;
	}

	long long decimal_part = 0;

	while (*nptr && *nptr != '.') {
		if (*nptr < '0' || *nptr > '9') {
			break;
		}
		decimal_part *= 10LL;
		decimal_part += (long long)(*nptr - '0');
		nptr++;
	}

	double sub_part = 0;
	double multiplier = 0.1;

	if (*nptr == '.') {
		nptr++;

		while (*nptr) {
			if (*nptr < '0' || *nptr > '9') {
				break;
			}

			sub_part += multiplier * (*nptr - '0');
			multiplier *= 0.1;
			nptr++;
		}
	}

	double expn = (double)sign;

	if (endptr) {
		*endptr = (char *)nptr;
	}
	double result = ((double)decimal_part + sub_part) * expn;
	return result;
}

FILE * stdout = NULL;
FILE * stderr = NULL;
FILE * stdin  = NULL;

int fputc(int c, FILE * stream) {
	if (stream == stdout) {
		char tmp[2] = {c,0};
		print_(tmp);
	}
	return c;
}

int fputs(const char * s, FILE * stream) {
	while (*s) {
		fputc(*s,stream);
		s++;
	}
	return 0;
}

int puts(const char * s) {
	fputs(s,stdout);
	fputc('\n',stdout);
	return 0;
}

static void print_dec(unsigned long long value, unsigned int width, char * buf, int * ptr, int fill_zero, int align_right, int precision) {
	unsigned long long n_width = 1;
	unsigned long long i = 9;
	if (precision == -1) precision = 1;

	if (value == 0) {
		n_width = 0;
	} else {
		unsigned long long val = value;
		while (val > 10) {
			val /= 10;
			n_width++;
		}
	}

	if (n_width < (unsigned long long)precision) n_width = precision;

	int printed = 0;
	if (align_right) {
		while (n_width + printed < width) {
			buf[*ptr] = fill_zero ? '0' : ' ';
			*ptr += 1;
			printed += 1;
		}

		i = n_width;
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			buf[*ptr + i - 1] = r + '0';
			i--;
			value = n;
		}
		*ptr += n_width;
	} else {
		i = n_width;
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			buf[*ptr + i - 1] = r + '0';
			i--;
			value = n;
			printed++;
		}
		*ptr += n_width;
		while (printed < (long long)width) {
			buf[*ptr] = fill_zero ? '0' : ' ';
			*ptr += 1;
			printed += 1;
		}
	}
}

/*
 * Hexadecimal to string
 */
static void print_hex(unsigned long long value, unsigned int width, char * buf, int * ptr) {
	int i = width;

	if (i == 0) i = 8;

	unsigned long long n_width = 1;
	unsigned long long j = 0x0F;
	while (value > j && j < UINT64_MAX) {
		n_width += 1;
		j *= 0x10;
		j += 0x0F;
	}

	while (i > (long long)n_width) {
		buf[*ptr] = '0';
		*ptr += 1;
		i--;
	}

	i = (long long)n_width;
	while (i-- > 0) {
		buf[*ptr] = "0123456789abcdef"[(value>>(i*4))&0xF];
		*ptr += + 1;
	}
}

/*
 * vasprintf()
 */
int xvasprintf(char * buf, const char * fmt, va_list args) {
	int i = 0;
	char * s;
	char * b = buf;
	int precision = -1;
	for (const char *f = fmt; *f; f++) {
		if (*f != '%') {
			*b++ = *f;
			continue;
		}
		++f;
		unsigned int arg_width = 0;
		int align = 1; /* right */
		int fill_zero = 0;
		int big = 0;
		int alt = 0;
		int always_sign = 0;
		while (1) {
			if (*f == '-') {
				align = 0;
				++f;
			} else if (*f == '#') {
				alt = 1;
				++f;
			} else if (*f == '*') {
				arg_width = (char)va_arg(args, int);
				++f;
			} else if (*f == '0') {
				fill_zero = 1;
				++f;
			} else if (*f == '+') {
				always_sign = 1;
				++f;
			} else {
				break;
			}
		}
		while (*f >= '0' && *f <= '9') {
			arg_width *= 10;
			arg_width += *f - '0';
			++f;
		}
		if (*f == '.') {
			++f;
			precision = 0;
			if (*f == '*') {
				precision = (int)va_arg(args, int);
				++f;
			} else  {
				while (*f >= '0' && *f <= '9') {
					precision *= 10;
					precision += *f - '0';
					++f;
				}
			}
		}
		if (*f == 'l') {
			big = 1;
			++f;
			if (*f == 'l') {
				big = 2;
				++f;
			}
		}
		if (*f == 'z') {
			big = 1;
			++f;
		}
		/* fmt[i] == '%' */
		switch (*f) {
			case 's': /* String pointer -> String */
				{
					size_t count = 0;
					if (big) {
						return -1;
					} else {
						s = (char *)va_arg(args, char *);
						if (s == NULL) {
							s = "(null)";
						}
						if (precision >= 0) {
							while (*s && precision > 0) {
								*b++ = *s++;
								count++;
								precision--;
								if (arg_width && count == arg_width) break;
							}
						} else {
							while (*s) {
								*b++ = *s++;
								count++;
								if (arg_width && count == arg_width) break;
							}
						}
					}
					while (count < arg_width) {
						*b++ = ' ';
						count++;
					}
				}
				break;
			case 'c': /* Single character */
				*b++ = (char)va_arg(args, int);
				break;
			case 'p':
				if (!arg_width) {
					arg_width = 8;
					alt = 1;
					if (sizeof(void*) == sizeof(long long)) big = 2;
				}
			case 'x': /* Hexadecimal number */
				{
					if (alt) {
						*b++ = '0';
						*b++ = 'x';
					}
					i = b - buf;
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					print_hex(val, arg_width, buf, &i);
					b = buf + i;
				}
				break;
			case 'i':
			case 'd': /* Decimal number */
				{
					long long val;
					if (big == 2) {
						val = (long long)va_arg(args, long long);
					} else if (big == 1) {
						val = (long)va_arg(args, long);
					} else {
						val = (int)va_arg(args, int);
					}
					if (val < 0) {
						*b++ = '-';
						val = -val;
					} else if (always_sign) {
						*b++ = '+';
					}
					i = b - buf;
					print_dec(val, arg_width, buf, &i, fill_zero, align, precision);
					b = buf + i;
				}
				break;
			case 'u': /* Unsigned ecimal number */
				i = b - buf;
				{
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					print_dec(val, arg_width, buf, &i, fill_zero, align, precision);
				}
				b = buf + i;
				break;
			case 'g': /* supposed to also support e */
			case 'f':
				{
					if (precision == -1) precision = 8;
					double val = (double)va_arg(args, double);
					if (val < 0) {
						*b++ = '-';
						val = -val;
					}
					i = b - buf;
					print_dec((long)val, arg_width, buf, &i, fill_zero, align, 1);
					b = buf + i;
					*b++ = '.';
					i = b - buf;
					for (int j = 0; j < ((precision > -1 && precision < 8) ? precision : 8); ++j) {
						if ((int)(val * 100000.0) % 100000 == 0 && j != 0) break;
						val = val - (int)val;
						val *= 10.0;
						print_dec((int)(val) % 10, 0, buf, &i, 0, 0, 1);
					}
					b = buf + i;
				}
				break;
			case '%': /* Escape */
				*b++ = '%';
				break;
			default: /* Nothing at all, just dump it */
				*b++ = *f;
				break;
		}
	}
	/* Ensure the buffer ends in a null */
	*b = '\0';
	return b - buf;
}


int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	return xvasprintf(str, format, ap);
}

int snprintf(char * str, size_t size, const char * format, ...) {
	(void)size;
	va_list args;
	va_start(args, format);
	int out = xvasprintf(str, format, args);
	va_end(args);
	return out;
}

int fprintf(FILE *stream, const char * fmt, ...) {
	static char str[1024] = {0};
	va_list args;
	va_start(args, fmt);
	int out = xvasprintf(str, fmt, args);
	fputs(str,stream);
	va_end(args);
	return out;
}

size_t strlen(const char *s) {
	size_t out = 0;
	while (*s) {
		out++;
		s++;
	}
	return out;
}
char * strdup(const char * src) {
	char * out = malloc(strlen(src)+1);
	char * c = out;
	while (*src) {
		*c = *src;
		c++; src++;
	}
	*c = 0;
	return out;
}

char *strstr(const char *haystack, const char *needle) {
	size_t s = strlen(needle);
	const char * end = haystack + strlen(haystack);

	while (haystack + s <= end) {
		if (!memcmp(haystack,needle,s)) return (char*)haystack;
		haystack++;
	}

	return NULL;
}

char * strcat(char *dest, const char *src) {
	char * end = dest;
	while (*end != '\0') {
		++end;
	}
	while (*src) {
		*end = *src;
		end++;
		src++;
	}
	*end = '\0';
	return dest;
}
