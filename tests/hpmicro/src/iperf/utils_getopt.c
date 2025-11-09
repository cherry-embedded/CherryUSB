/*
 * This file is derived from musl v1.2.0.
 * Modifications are applied.
 * Copyright (C) Bouffalo Lab 2016-2020
 */

/*
 * Copyright © 2005-2020 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <utils_getopt.h>

int utils_getopt_init(getopt_env_t *env, int opterr)
{
    if (!env) {
        return -1;
    }
    env->optarg = NULL;
    env->optind = 1;
    env->opterr = opterr;
    env->optopt = 0;
    env->__optpos = 0;
    return 0;
}

#define NEWLINE "\r\n"

int utils_getopt(getopt_env_t *env, int argc, char *const argv[], const char *optstring)
{
    int i;
    char c, d;
    char *optchar;

    if (!env) {
        return -1;
    }

    if (env->optind >= argc || !argv[env->optind])
        return -1;

    if (argv[env->optind][0] != '-') {
        if (optstring[0] == '-') {
            env->optarg = argv[env->optind++];
            return 1;
        }
        return -1;
    }

    if (!argv[env->optind][1])
        return -1;

    if (argv[env->optind][1] == '-' && !argv[env->optind][2])
        return env->optind++, -1;

    if (!env->__optpos)
        env->__optpos++;
    c = argv[env->optind][env->__optpos];
    optchar = argv[env->optind] + env->__optpos;
    env->__optpos += !!c;

    if (!argv[env->optind][env->__optpos]) {
        env->optind++;
        env->__optpos = 0;
    }

    if (optstring[0] == '-' || optstring[0] == '+')
        optstring++;

    i = 0;
    do
        d = optstring[i++];
    while (d && d != c);

    if (d != c || c == ':') {
        env->optopt = c;
        if (optstring[0] != ':' && env->opterr)
            printf("%s: unrecognized option: %c" NEWLINE, argv[0], *optchar);
        return '?';
    }
    if (optstring[i] == ':') {
        env->optarg = 0;
        if (optstring[i + 1] != ':' || env->__optpos) {
            env->optarg = argv[env->optind++] + env->__optpos;
            env->__optpos = 0;
        }
        if (env->optind > argc) {
            env->optopt = c;
            if (optstring[0] == ':')
                return ':';
            if (env->opterr) {
                printf("%s: option requires an argument: %c" NEWLINE, argv[0], *optchar);
            }
            return '?';
        }
    }
    return c;
}

static int params_filter(char **params, uint32_t *r)
{
    char *p;
    uint32_t result = 0;
    uint8_t base = 0;

    p = *params;

    if ((*p == '0') && ((*(p + 1) == 'x') || (*(p + 1) == 'X'))) {
        p = p + 2;
        base = 16;

    } else {
        base = 10;
    }

    while (*p) {
        result *= base;
        if (*p >= '0' && *p <= '9')
            result += *p - '0';
        else if (base == 10)
            return -1;

        if (base == 16) {
            if (*p >= 'a' && *p <= 'f')
                result += *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F')
                result += *p - 'A' + 10;
        }
        p++;
    }

    *r = result;
    return 0;
}

void get_bytearray_from_string(char **params, uint8_t *result, int array_size)
{
    int i = 0;
    char rand[3];

    for (i = 0; i < array_size; i++) {
        memcpy(rand, *params, 2);
        rand[2] = '\0';
        result[i] = strtol(rand, NULL, 16);
        *params = *params + 2;
    }
}

void get_uint8_from_string(char **params, uint8_t *result)
{
    uint32_t p = 0;
    int state = 0;

    state = params_filter(params, &p);
    if (!state) {
        *result = p & 0xff;
    } else
        *result = 0;
}

void get_uint16_from_string(char **params, uint16_t *result)
{
    uint32_t p = 0;
    int state = 0;

    state = params_filter(params, &p);
    if (!state) {
        *result = p & 0xffff;
    } else
        *result = 0;
}

void get_uint32_from_string(char **params, uint32_t *result)
{
    uint32_t p = 0;
    int state = 0;

    state = params_filter(params, &p);
    if (!state) {
        *result = p;
    } else
        *result = 0;
}

void utils_parse_number(const char *str, char sep, uint8_t *buf, int buflen, int base)
{
    int i;
    for (i = 0; i < buflen; i++) {
        buf[i] = (uint8_t)strtol(str, NULL, base);
        str = strchr(str, sep);
        if (str == NULL || *str == '\0') {
            break;
        }
        str++;
    }
}

void utils_parse_number_adv(const char *str, char sep, uint8_t *buf, int buflen, int base, int *count)
{
    int i;

    for (i = 0; i < buflen; i++) {
        buf[i] = (uint8_t)strtol(str, NULL, base);
        str = strchr(str, sep);
        if (str == NULL || *str == '\0') {
            break;
        }
        str++;
    }
    *count = (i + 1);
}

unsigned long long convert_arrayToU64(uint8_t *inputArray)
{
    unsigned long long result = 0;
    for (uint8_t i = 0; i < 8; i++) {
        result <<= 8;
        result |= (unsigned long long)inputArray[7 - i];
    }

    return result;
}

void convert_u64ToArray(unsigned long long inputU64, uint8_t result[8])
{
    for (int i = 0; i < 8; i++) {
        result[i] = inputU64 >> (i * 8);
    }
}

void utils_memdrain8(void *src, size_t len)
{
    volatile uint8_t *s = (uint8_t *)src;
    uint8_t tmp;

    while (len--) {
        tmp = *s++;
    }

    (void)tmp;
}

void utils_memdrain16(void *src, size_t len)
{
    volatile uint16_t *s = (uint16_t *)src;
    uint16_t tmp;

    len >>= 1; //convert to half words

    while (len--) {
        tmp = *s++;
    }

    (void)tmp;
}

void utils_memdrain32(void *src, size_t len)
{
    volatile uint32_t *s = (uint32_t *)src;
    uint32_t tmp;

    len >>= 2; //convert to words

    while (len--) {
        tmp = *s++;
    }

    (void)tmp;
}

void utils_memdrain64(void *src, size_t len)
{
    volatile uint64_t *s = (uint64_t *)src;
    uint64_t tmp;

    len >>= 3; //convert to two words

    while (len--) {
        tmp = *s++;
    }

    (void)tmp;
}

void *utils_memdrain8_with_check(void *src, size_t len, uint8_t seq)
{
    volatile uint8_t *s = (uint8_t *)src;
    uint8_t tmp;

    (void)tmp;

    while (len--) {
        tmp = *s++;
        if ((seq++) != tmp) {
            return (uint8_t *)s - 1;
        }
    }

    return NULL;
}

void *utils_memdrain16_with_check(void *src, size_t len, uint16_t seq)
{
    volatile uint16_t *s = (uint16_t *)src;
    uint16_t tmp;
    (void)tmp;

    len >>= 1; //convert to half words

    while (len--) {
        tmp = *s++;
        if ((seq++) != tmp) {
            return (uint16_t *)s - 1;
        }
    }

    return NULL;
}

void *utils_memdrain32_with_check(void *src, size_t len, uint32_t seq)
{
    volatile uint32_t *s = (uint32_t *)src;
    uint32_t tmp;
    (void)tmp;

    len >>= 2; //convert to words

    while (len--) {
        tmp = *s++;
        if ((seq++) != tmp) {
            return (uint32_t *)s - 1;
        }
    }

    return NULL;
}

void *utils_memdrain64_with_check(void *src, size_t len, uint64_t seq)
{
    volatile uint64_t *s = (uint64_t *)src;
    uint64_t tmp;
    (void)tmp;

    len >>= 3; //convert to two words

    while (len--) {
        tmp = *s++;
        if ((seq++) != tmp) {
            return (uint64_t *)s - 1;
        }
    }

    return NULL;
}
