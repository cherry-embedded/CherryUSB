/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer, Richard.hmm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi_utils.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  Conversion routines and string manipulation routines
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "utils_private.h"
#include "scpi/utils.h"

static size_t patternSeparatorShortPos(const char * pattern, size_t len);
static size_t patternSeparatorPos(const char * pattern, size_t len);
static size_t cmdSeparatorPos(const char * cmd, size_t len);

/**
 * Find the first occurrence in str of a character in set.
 * @param str
 * @param size
 * @param set
 * @return
 */
char * strnpbrk(const char *str, size_t size, const char *set) {
    const char *scanp;
    long c, sc;
    const char * strend = str + size;

    while ((strend != str) && ((c = *str++) != 0)) {
        for (scanp = set; (sc = *scanp++) != '\0';)
            if (sc == c)
                return ((char *) (str - 1));
    }
    return (NULL);
}

/**
 * Converts signed/unsigned 32 bit integer value to string in specific base
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @param base  output base
 * @param sign
 * @return number of bytes written to str (without '\0')
 */
size_t UInt32ToStrBaseSign(uint32_t val, char * str, size_t len, int8_t base, scpi_bool_t sign) {
    const char digits[] = "0123456789ABCDEF";

#define ADD_CHAR(c) if (pos < len) str[pos++] = (c)
    uint32_t x = 0;
    int_fast8_t digit;
    size_t pos = 0;
    uint32_t uval = val;

    if (uval == 0) {
        ADD_CHAR('0');
    } else {

        switch (base) {
            case 2:
                x = 0x80000000L;
                break;
            case 8:
                x = 0x40000000L;
                break;
            default:
            case 10:
                base = 10;
                x = 1000000000L;
                break;
            case 16:
                x = 0x10000000L;
                break;
        }

        /* add sign for numbers in base 10 */
        if (sign && ((int32_t) val < 0) && (base == 10)) {
            uval = -val;
            ADD_CHAR('-');
        }

        /* remove leading zeros */
        while ((uval / x) == 0) {
            x /= base;
        }

        do {
            digit = (uint8_t) (uval / x);
            ADD_CHAR(digits[digit]);
            uval -= digit * x;
            x /= base;
        } while (x && (pos < len));
    }

    if (pos < len) str[pos] = 0;
    return pos;
#undef ADD_CHAR
}

/**
 * Converts signed 32 bit integer value to string
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_Int32ToStr(int32_t val, char * str, size_t len) {
    return UInt32ToStrBaseSign((uint32_t) val, str, len, 10, TRUE);
}

/**
 * Converts unsigned 32 bit integer value to string in specific base
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @param base  output base
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_UInt32ToStrBase(uint32_t val, char * str, size_t len, int8_t base) {
    return UInt32ToStrBaseSign(val, str, len, base, FALSE);
}

/**
 * Converts signed/unsigned 64 bit integer value to string in specific base
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @param base  output base
 * @param sign
 * @return number of bytes written to str (without '\0')
 */
size_t UInt64ToStrBaseSign(uint64_t val, char * str, size_t len, int8_t base, scpi_bool_t sign) {
    const char digits[] = "0123456789ABCDEF";

#define ADD_CHAR(c) if (pos < len) str[pos++] = (c)
    uint64_t x = 0;
    int_fast8_t digit;
    size_t pos = 0;
    uint64_t uval = val;

    if (uval == 0) {
        ADD_CHAR('0');
    } else {

        switch (base) {
            case 2:
                x = 0x8000000000000000ULL;
                break;
            case 8:
                x = 0x8000000000000000ULL;
                break;
            default:
            case 10:
                x = 10000000000000000000ULL;
                base = 10;
                break;
            case 16:
                x = 0x1000000000000000ULL;
                break;
        }

        /* add sign for numbers in base 10 */
        if (sign && ((int64_t) val < 0) && (base == 10)) {
            uval = -val;
            ADD_CHAR('-');
        }

        /* remove leading zeros */
        while ((uval / x) == 0) {
            x /= base;
        }

        do {
            digit = (uint8_t) (uval / x);
            ADD_CHAR(digits[digit]);
            uval -= digit * x;
            x /= base;
        } while (x && (pos < len));
    }

    if (pos < len) str[pos] = 0;
    return pos;
#undef ADD_CHAR
}

/**
 * Converts signed 64 bit integer value to string
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_Int64ToStr(int64_t val, char * str, size_t len) {
    return UInt64ToStrBaseSign((uint64_t) val, str, len, 10, TRUE);
}

/**
 * Converts signed/unsigned 64 bit integer value to string in specific base
 * @param val   integer value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @param base  output base
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_UInt64ToStrBase(uint64_t val, char * str, size_t len, int8_t base) {
    return UInt64ToStrBaseSign(val, str, len, base, FALSE);
}

/**
 * Converts float (32 bit) value to string
 * @param val   long value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_FloatToStr(float val, char * str, size_t len) {
    SCPIDEFINE_floatToStr(val, str, len);
    return strlen(str);
}

/**
 * Converts double (64 bit) value to string
 * @param val   double value
 * @param str   converted textual representation
 * @param len   string buffer length
 * @return number of bytes written to str (without '\0')
 */
size_t SCPI_DoubleToStr(double val, char * str, size_t len) {
    SCPIDEFINE_doubleToStr(val, str, len);
    return strlen(str);
}

/**
 * Converts string to signed 32bit integer representation
 * @param str   string value
 * @param val   32bit integer result
 * @return      number of bytes used in string
 */
size_t strBaseToInt32(const char * str, int32_t * val, int8_t base) {
    char * endptr;
    *val = strtol(str, &endptr, base);
    return endptr - str;
}

/**
 * Converts string to unsigned 32bit integer representation
 * @param str   string value
 * @param val   32bit integer result
 * @return      number of bytes used in string
 */
size_t strBaseToUInt32(const char * str, uint32_t * val, int8_t base) {
    char * endptr;
    *val = strtoul(str, &endptr, base);
    return endptr - str;
}

/**
 * Converts string to signed 64bit integer representation
 * @param str   string value
 * @param val   64bit integer result
 * @return      number of bytes used in string
 */
size_t strBaseToInt64(const char * str, int64_t * val, int8_t base) {
    char * endptr;
    *val = SCPIDEFINE_strtoll(str, &endptr, base);
    return endptr - str;
}

/**
 * Converts string to unsigned 64bit integer representation
 * @param str   string value
 * @param val   64bit integer result
 * @return      number of bytes used in string
 */
size_t strBaseToUInt64(const char * str, uint64_t * val, int8_t base) {
    char * endptr;
    *val = SCPIDEFINE_strtoull(str, &endptr, base);
    return endptr - str;
}

/**
 * Converts string to float (32 bit) representation
 * @param str   string value
 * @param val   float result
 * @return      number of bytes used in string
 */
size_t strToFloat(const char * str, float * val) {
    char * endptr;
    *val = SCPIDEFINE_strtof(str, &endptr);
    return endptr - str;
}

/**
 * Converts string to double (64 bit) representation
 * @param str   string value
 * @param val   double result
 * @return      number of bytes used in string
 */
size_t strToDouble(const char * str, double * val) {
    char * endptr;
    *val = strtod(str, &endptr);
    return endptr - str;
}

/**
 * Compare two strings with exact length
 * @param str1
 * @param len1
 * @param str2
 * @param len2
 * @return TRUE if len1==len2 and "len" characters of both strings are equal
 */
scpi_bool_t compareStr(const char * str1, size_t len1, const char * str2, size_t len2) {
    if (len1 != len2) {
        return FALSE;
    }

    if (SCPIDEFINE_strncasecmp(str1, str2, len2) == 0) {
        return TRUE;
    }

    return FALSE;
}

/**
 * Compare two strings, one be longer but may contains only numbers in that section
 * @param str1
 * @param len1
 * @param str2
 * @param len2
 * @return TRUE if strings match
 */
scpi_bool_t compareStrAndNum(const char * str1, size_t len1, const char * str2, size_t len2, int32_t * num) {
    scpi_bool_t result = FALSE;
    size_t i;

    if (len2 < len1) {
        return FALSE;
    }

    if (SCPIDEFINE_strncasecmp(str1, str2, len1) == 0) {
        result = TRUE;

        if (num) {
            if (len1 == len2) {
                /* *num = 1; */
            } else {
                int32_t tmpNum;
                i = len1 + strBaseToInt32(str2 + len1, &tmpNum, 10);
                if (i != len2) {
                    result = FALSE;
                } else {
                    *num = tmpNum;
                }
            }
        } else {
            for (i = len1; i < len2; i++) {
                if (!isdigit((int) str2[i])) {
                    result = FALSE;
                    break;
                }
            }
        }
    }

    return result;
}

/**
 * Count white spaces from the beggining
 * @param cmd - command
 * @param len - max search length
 * @return number of white spaces
 */
size_t skipWhitespace(const char * cmd, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (!isspace((unsigned char) cmd[i])) {
            return i;
        }
    }
    return len;
}

/**
 * Pattern is composed from upper case an lower case letters. This function
 * search the first lowercase letter
 * @param pattern
 * @param len - max search length
 * @return position of separator or len
 */
static size_t patternSeparatorShortPos(const char * pattern, size_t len) {
    size_t i;
    for (i = 0; (i < len) && pattern[i]; i++) {
        if (islower((unsigned char) pattern[i])) {
            return i;
        }
    }
    return i;
}

/**
 * Find pattern separator position
 * @param pattern
 * @param len - max search length
 * @return position of separator or len
 */
static size_t patternSeparatorPos(const char * pattern, size_t len) {

    char * separator = strnpbrk(pattern, len, "?:[]");
    if (separator == NULL) {
        return len;
    } else {
        return separator - pattern;
    }
}

/**
 * Find command separator position
 * @param cmd - input command
 * @param len - max search length
 * @return position of separator or len
 */
static size_t cmdSeparatorPos(const char * cmd, size_t len) {
    char * separator = strnpbrk(cmd, len, ":?");
    size_t result;
    if (separator == NULL) {
        result = len;
    } else {
        result = separator - cmd;
    }

    return result;
}

/**
 * Match pattern and str. Pattern is in format UPPERCASElowercase
 * @param pattern
 * @param pattern_len
 * @param str
 * @param str_len
 * @return
 */
scpi_bool_t matchPattern(const char * pattern, size_t pattern_len, const char * str, size_t str_len, int32_t * num) {
    int pattern_sep_pos_short;

    if ((pattern_len > 0) && pattern[pattern_len - 1] == '#') {
        size_t new_pattern_len = pattern_len - 1;

        pattern_sep_pos_short = patternSeparatorShortPos(pattern, new_pattern_len);

        return compareStrAndNum(pattern, new_pattern_len, str, str_len, num) ||
                compareStrAndNum(pattern, pattern_sep_pos_short, str, str_len, num);
    } else {

        pattern_sep_pos_short = patternSeparatorShortPos(pattern, pattern_len);

        return compareStr(pattern, pattern_len, str, str_len) ||
                compareStr(pattern, pattern_sep_pos_short, str, str_len);
    }
}

/**
 * Compare pattern and command
 * @param pattern eg. [:MEASure]:VOLTage:DC?
 * @param cmd - command
 * @param len - max search length
 * @return TRUE if pattern matches, FALSE otherwise
 */
scpi_bool_t matchCommand(const char * pattern, const char * cmd, size_t len, int32_t *numbers, size_t numbers_len, int32_t default_value) {
#define SKIP_PATTERN(n) do {pattern_ptr += (n);  pattern_len -= (n);} while(0)
#define SKIP_CMD(n) do {cmd_ptr += (n);  cmd_len -= (n);} while(0)

    scpi_bool_t result = FALSE;
    int brackets = 0;
    int cmd_sep_pos = 0;

    size_t numbers_idx = 0;
    int32_t *number_ptr = NULL;

    const char * pattern_ptr = pattern;
    int pattern_len = strlen(pattern);

    const char * cmd_ptr = cmd;
    size_t cmd_len = SCPIDEFINE_strnlen(cmd, len);

    /* both commands are query commands? */
    if (pattern_ptr[pattern_len - 1] == '?') {
        if (cmd_ptr[cmd_len - 1] == '?') {
            cmd_len -= 1;
            pattern_len -= 1;
        } else {
            return FALSE;
        }
    }

    /* now support optional keywords in pattern style, e.g. [:MEASure]:VOLTage:DC? */
    if (pattern_ptr[0] == '[') { /* skip first '[' */
        SKIP_PATTERN(1);
        brackets++;
    }
    if (pattern_ptr[0] == ':') { /* skip first ':' */
        SKIP_PATTERN(1);
    }

    if (cmd_ptr[0] == ':') {
        /* handle errornouse ":*IDN?" */
        if (cmd_len >= 2) {
            if (cmd_ptr[1] != '*') {
                SKIP_CMD(1);
            } else {
                return FALSE;
            }
        }
    }

    while (1) {
        int pattern_sep_pos = patternSeparatorPos(pattern_ptr, pattern_len);

        cmd_sep_pos = cmdSeparatorPos(cmd_ptr, cmd_len);

        if ((pattern_sep_pos > 0) && pattern_ptr[pattern_sep_pos - 1] == '#') {
            if (numbers && (numbers_idx < numbers_len)) {
                number_ptr = numbers + numbers_idx;
                *number_ptr = default_value; /* default value */
            } else {
                number_ptr = NULL;
            }
            numbers_idx++;
        } else {
            number_ptr = NULL;
        }

        if (matchPattern(pattern_ptr, pattern_sep_pos, cmd_ptr, cmd_sep_pos, number_ptr)) {
            SKIP_PATTERN(pattern_sep_pos);
            SKIP_CMD(cmd_sep_pos);
            result = TRUE;

            /* command is complete */
            if ((pattern_len == 0) && (cmd_len == 0)) {
                break;
            }

            /* pattern complete, but command not */
            if ((pattern_len == 0) && (cmd_len > 0)) {
                result = FALSE;
                break;
            }

            /* command complete, but pattern not */
            if (cmd_len == 0) {
                /* verify all subsequent pattern parts are also optional */
                while (pattern_len) {
                    pattern_sep_pos = patternSeparatorPos(pattern_ptr, pattern_len);
                    switch (pattern_ptr[pattern_sep_pos]) {
                        case '[':
                            brackets++;
                            break;
                        case ']':
                            brackets--;
                            break;
                        default:
                            break;
                    }
                    SKIP_PATTERN(pattern_sep_pos + 1);
                    if (brackets == 0) {
                        if ((pattern_len > 0) && (pattern_ptr[0] == '[')) {
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                if (pattern_len != 0) {
                    result = FALSE;
                }
                break; /* exist optional keyword, command is complete */
            }

            /* both command and patter contains command separator at this position */
            if ((pattern_len > 0)
                    && ((pattern_ptr[0] == cmd_ptr[0])
                    && (pattern_ptr[0] == ':'))) {
                SKIP_PATTERN(1);
                SKIP_CMD(1);
            } else if ((pattern_len > 1)
                    && (pattern_ptr[1] == cmd_ptr[0])
                    && (pattern_ptr[0] == '[')
                    && (pattern_ptr[1] == ':')) {
                SKIP_PATTERN(2); /* for skip '[' in "[:" */
                SKIP_CMD(1);
                brackets++;
            } else if ((pattern_len > 1)
                    && (pattern_ptr[1] == cmd_ptr[0])
                    && (pattern_ptr[0] == ']')
                    && (pattern_ptr[1] == ':')) {
                SKIP_PATTERN(2); /* for skip ']' in "]:" */
                SKIP_CMD(1);
                brackets--;
            } else if ((pattern_len > 2)
                    && (pattern_ptr[2] == cmd_ptr[0])
                    && (pattern_ptr[0] == ']')
                    && (pattern_ptr[1] == '[')
                    && (pattern_ptr[2] == ':')) {
                SKIP_PATTERN(3); /* for skip '][' in "][:" */
                SKIP_CMD(1);
                /* brackets++; */
                /* brackets--; */
            } else {
                result = FALSE;
                break;
            }
        } else {
            SKIP_PATTERN(pattern_sep_pos);
            if ((pattern_ptr[0] == ']') && (pattern_ptr[1] == ':')) {
                SKIP_PATTERN(2); /* for skip ']' in "]:" , pattern_ptr continue, while cmd_ptr remain unchanged */
                brackets--;
            } else if ((pattern_len > 2) && (pattern_ptr[0] == ']')
                    && (pattern_ptr[1] == '[')
                    && (pattern_ptr[2] == ':')) {
                SKIP_PATTERN(3); /* for skip ']' in "][:" , pattern_ptr continue, while cmd_ptr remain unchanged */
                /* brackets++; */
                /* brackets--; */
            } else {
                result = FALSE;
                break;
            }
        }
    }

    return result;
#undef SKIP_PATTERN
#undef SKIP_CMD
}

/**
 * Compose command from previous command anc current command
 *
 * @param prev pointer to previous command
 * @param current pointer of current command
 *
 * prev and current should be in the same memory buffer
 */
scpi_bool_t composeCompoundCommand(const scpi_token_t * prev, scpi_token_t * current) {
    size_t i;

    /* Invalid input */
    if (current == NULL || current->ptr == NULL || current->len == 0)
        return FALSE;

    /* no previous command - nothing to do*/
    if (prev->ptr == NULL || prev->len == 0)
        return TRUE;

    /* Common command or command root - nothing to do */
    if (current->ptr[0] == '*' || current->ptr[0] == ':')
        return TRUE;

    /* Previsou command was common command - nothing to do */
    if (prev->ptr[0] == '*')
        return TRUE;

    /* Find last occurence of ':' */
    for (i = prev->len; i > 0; i--) {
        if (prev->ptr[i - 1] == ':') {
            break;
        }
    }

    /* Previous command was simple command - nothing to do*/
    if (i == 0)
        return TRUE;

    current->ptr -= i;
    current->len += i;
    memmove(current->ptr, prev->ptr, i);
    return TRUE;
}



#if !HAVE_STRNLEN
/* use FreeBSD strnlen */

/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
 * All rights reserved.
 */
size_t
BSD_strnlen(const char *s, size_t maxlen) {
    size_t len;

    for (len = 0; len < maxlen; len++, s++) {
        if (!*s)
            break;
    }
    return (len);
}
#endif

#if !HAVE_STRNCASECMP && !HAVE_STRNICMP

int OUR_strncasecmp(const char *s1, const char *s2, size_t n) {
    unsigned char c1, c2;

    for (; n != 0; n--) {
        c1 = tolower((unsigned char) *s1++);
        c2 = tolower((unsigned char) *s2++);
        if (c1 != c2) {
            return c1 - c2;
        }
        if (c1 == '\0') {
            return 0;
        }
    }
    return 0;
}
#endif

#if USE_MEMORY_ALLOCATION_FREE && !HAVE_STRNDUP
char *OUR_strndup(const char *s, size_t n) {
    size_t len = SCPIDEFINE_strnlen(s, n);
    char * result = malloc(len + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}
#endif

#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION && !USE_MEMORY_ALLOCATION_FREE

/**
 * Initialize heap structure
 * @param heap - pointer to manual allocated heap buffer
 * @param error_info_heap - buffer for the heap
 * @param error_info_heap_length - length of the heap
 */
void scpiheap_init(scpi_error_info_heap_t * heap, char * error_info_heap, size_t error_info_heap_length)
{
    heap->data = error_info_heap;
    heap->wr = 0;
    heap->size = error_info_heap_length;
    heap->count = heap->size;
    memset(heap->data, 0, heap->size);
}

/**
 * Duplicate string if "strdup" ("malloc/free") not supported on system.
 * Allocate space in heap if it possible
 *
 * @param heap - pointer to manual allocated heap buffer
 * @param s - current pointer of duplication string
 * @return - pointer of duplicated string or NULL, if duplicate is not possible.
 */
char * scpiheap_strndup(scpi_error_info_heap_t * heap, const char *s, size_t n) {
    if (!s || !heap || !heap->size) {
        return NULL;
    }

    if (heap->data[heap->wr] != '\0') {
        return NULL;
    }

    if (*s == '\0') {
        return NULL;
    }

    size_t len = SCPIDEFINE_strnlen(s, n) + 1; /* additional '\0' at end */
    if (len > heap->count) {
        return NULL;
    }
    const char * ptrs = s;
    char * head = &heap->data[heap->wr];
    size_t rem = heap->size - (&heap->data[heap->wr] - heap->data);

    if (len >= rem) {
        memcpy(&heap->data[heap->wr], s, rem);
        len = len - rem;
        ptrs += rem;
        heap->wr = 0;
        heap->count -= rem;
    }

    memcpy(&heap->data[heap->wr], ptrs, len);
    heap->wr += len;
    heap->count -= len;

    /* ensure '\0' a the end */
    if (heap->wr > 0) {
        heap->data[heap->wr - 1] = '\0';
    } else {
        heap->data[heap->size - 1] = '\0';
    }
    return head;
}

/**
 * Return pointers and lengths two parts of string in the circular buffer from heap
 *
 * @param heap - pointer to manual allocated heap buffer
 * @param s - pointer of duplicate string.
 * @return len1 - lenght of first part of string.
 * @return s2 - pointer of second part of string, if string splited .
 * @return len2 - lenght of second part of string.
 */
scpi_bool_t scpiheap_get_parts(scpi_error_info_heap_t * heap, const char * s, size_t * len1, const char ** s2, size_t * len2) {
    if (!heap || !s || !len1 || !s2 || !len2) {
        return FALSE;
    }

    if (*s == '\0') {
        return FALSE;
    }

    *len1 = 0;
    size_t rem = heap->size - (s - heap->data);
    *len1 = strnlen(s, rem);

    if (&s[*len1 - 1] == &heap->data[heap->size - 1]) {
        *s2 = heap->data;
        *len2 = strnlen(*s2, heap->size);
    } else {
        *s2 = NULL;
        *len2 = 0;
    }
    return TRUE;
}

/**
 * Frees space in heap, if "malloc/free" not supported on system, or nothing.
 *
 * @param heap - pointer to manual allocated heap buffer
 * @param s - pointer of duplicate string
 * @param rollback - backward write pointer in heap
 */
void scpiheap_free(scpi_error_info_heap_t * heap, char * s, scpi_bool_t rollback) {

    if (!s) return;

    char * data_add;
    size_t len[2];

    if (!scpiheap_get_parts(heap, s, &len[0], (const char **)&data_add, &len[1])) return;

    if (data_add) {
        len[1]++;
        memset(data_add, 0, len[1]);
        heap->count += len[1];
    } else {
        len[0]++;
    }
    memset(s, 0, len[0]);
    heap->count += len[0];
    if (heap->count == heap->size) {
        heap->wr = 0;
        return;
    }
    if (rollback) {
        size_t rb = len[0] + len[1];
        if (rb > heap->wr) {
            heap->wr += heap->size;
        }
        heap->wr -= rb;
    }
}

#endif

/*
 * Floating point to string conversion routines
 *
 * Copyright (C) 2002 Michael Ringgaard. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static char *scpi_ecvt(double arg, int ndigits, int *decpt, int *sign, char *buf, size_t bufsize) {
    int r1, r2;
    double fi, fj;
    int w1, w2;

    if (ndigits < 0) ndigits = 0;
    if (ndigits >= (int) (bufsize - 1)) ndigits = bufsize - 2;

    r2 = 0;
    *sign = 0;
    w1 = 0;
    if (arg < 0) {
        *sign = 1;
        arg = -arg;
    }
    frexp(arg, &r1);
    arg = modf(arg, &fi);

    if (fi != 0) {
        r1 = r1 * 308 / 1024 - ndigits;
        w2 = bufsize;
        while (r1 > 0) {
            fj = modf(fi / 10, &fi);
            r2++;
            r1--;
        }
        while (fi != 0) {
            fj = modf(fi / 10, &fi);
            buf[--w2] = (int) ((fj + .03) * 10) + '0';
            r2++;
        }
        while (w2 < (int) bufsize) buf[w1++] = buf[w2++];
    } else if (arg > 0) {
        while ((fj = arg * 10) < 1) {
            arg = fj;
            r2--;
        }
    }
    w2 = ndigits;
    *decpt = r2;
    if (w2 < 0) {
        buf[0] = '\0';
        return buf;
    }
    while (w1 <= w2 && w1 < (int) bufsize) {
        arg *= 10;
        arg = modf(arg, &fj);
        buf[w1++] = (int) fj + '0';
    }
    if (w2 >= (int) bufsize) {
        buf[bufsize - 1] = '\0';
        return buf;
    }
    w1 = w2;
    buf[w2] += 5;
    while (buf[w2] > '9') {
        buf[w2] = '0';
        if (w2 > 0) {
            ++buf[--w2];
        } else {
            buf[w2] = '1';
            (*decpt)++;
        }
    }
    buf[w1] = '\0';
    return buf;
}

#define SCPI_DTOSTRE_BUFFER_SIZE 32

char * SCPI_dtostre(double __val, char * __s, size_t __ssize, unsigned char __prec, unsigned char __flags) {
    char buffer[SCPI_DTOSTRE_BUFFER_SIZE];

    int sign = SCPIDEFINE_signbit(__val);
    char * s = buffer;
    int decpt;
    if (sign) {
        __val = -__val;
        s[0] = '-';
        s++;
    } else if (!SCPIDEFINE_isnan(__val)) {
        if (SCPI_DTOSTRE_PLUS_SIGN & __flags) {
            s[0] = '+';
            s++;
        } else if (SCPI_DTOSTRE_ALWAYS_SIGN & __flags) {
            s[0] = ' ';
            s++;
        }
    }

    if (!SCPIDEFINE_isfinite(__val)) {
        if (SCPIDEFINE_isnan(__val)) {
            strcpy(s, (__flags & SCPI_DTOSTRE_UPPERCASE) ? "NAN" : "nan");
        } else {
            strcpy(s, (__flags & SCPI_DTOSTRE_UPPERCASE) ? "INF" : "inf");
        }
        strncpy(__s, buffer, __ssize);
        __s[__ssize - 1] = '\0';
        return __s;
    }

    scpi_ecvt(__val, __prec, &decpt, &sign, s, SCPI_DTOSTRE_BUFFER_SIZE - 1);
    if (decpt > 1 && decpt <= __prec) {
        memmove(s + decpt + 1, s + decpt, __prec + 1 - decpt);
        s[decpt] = '.';
        decpt = 0;
    } else if (decpt > -4 && decpt <= 0) {
        decpt = -decpt + 1;
        memmove(s + decpt + 1, s, __prec + 1);
        memset(s, '0', decpt + 1);
        s[1] = '.';
        decpt = 0;
    } else {
        memmove(s + 2, s + 1, __prec + 1);
        s[1] = '.';
        decpt--;
    }

    s = &s[__prec];
    while (s[0] == '0') {
        s[0] = 0;
        s--;
    }
    if (s[0] == '.') {
        s[0] = 0;
        s--;
    }

    if (decpt != 0) {
        s++;
        s[0] = 'e';
        s++;
        if (decpt != 0) {
            if (decpt > 0) {
                s[0] = '+';
            }
            if (decpt < 0) {
                s[0] = '-';
                decpt = -decpt;
            }
            s++;
        }
        UInt32ToStrBaseSign(decpt, s, 5, 10, 0);
        if (s[1] == 0) {
            s[2] = s[1];
            s[1] = s[0];
            s[0] = '0';
        }
    }

    strncpy(__s, buffer, __ssize);
    __s[__ssize - 1] = '\0';
    return __s;
}

/**
 * Get native CPU endiannes
 * @return
 */
scpi_array_format_t SCPI_GetNativeFormat(void) {

    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1 ? SCPI_FORMAT_BIGENDIAN : SCPI_FORMAT_LITTLEENDIAN;
}

/**
 * Swap 16bit number
 * @param val
 * @return
 */
uint16_t SCPI_Swap16(uint16_t val) {
    return ((val & 0x00FF) << 8) |
            ((val & 0xFF00) >> 8);
}

/**
 * Swap 32bit number
 * @param val
 * @return
 */
uint32_t SCPI_Swap32(uint32_t val) {
    return ((val & 0x000000FFul) << 24) |
            ((val & 0x0000FF00ul) << 8) |
            ((val & 0x00FF0000ul) >> 8) |
            ((val & 0xFF000000ul) >> 24);
}

/**
 * Swap 64bit number
 * @param val
 * @return
 */
uint64_t SCPI_Swap64(uint64_t val) {
    return ((val & 0x00000000000000FFull) << 56) |
            ((val & 0x000000000000FF00ull) << 40) |
            ((val & 0x0000000000FF0000ull) << 24) |
            ((val & 0x00000000FF000000ull) << 8) |
            ((val & 0x000000FF00000000ull) >> 8) |
            ((val & 0x0000FF0000000000ull) >> 24) |
            ((val & 0x00FF000000000000ull) >> 40) |
            ((val & 0xFF00000000000000ull) >> 56);
}
