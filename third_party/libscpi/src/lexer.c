/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
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
 * @file   lexer.c
 * @date   Wed Mar 20 19:35:19 UTC 2013
 * 
 * @brief  SCPI Lexer
 * 
 * 
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lexer_private.h"
#include "scpi/error.h"

/**
 * Is white space
 * @param c
 * @return 
 */
static int isws(int c) {
    if ((c == ' ') || (c == '\t')) {
        return 1;
    }
    return 0;
}

/**
 * Is binary digit
 * @param c
 * @return 
 */
static int isbdigit(int c) {
    if ((c == '0') || (c == '1')) {
        return 1;
    }
    return 0;
}

/**
 * Is hexadecimal digit
 * @param c
 * @return 
 */
static int isqdigit(int c) {
    if ((c == '0') || (c == '1') || (c == '2') || (c == '3') || (c == '4') || (c == '5') || (c == '6') || (c == '7')) {
        return 1;
    }
    return 0;
}

/**
 * Is end of string
 * @param state
 * @return 
 */
static int iseos(lex_state_t * state) {
    if ((state->buffer + state->len) <= (state->pos)) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Private export of iseos
 * @param state
 * @return 
 */
int scpiLex_IsEos(lex_state_t * state) {
    return iseos(state);
}

/**
 * Test current character
 * @param state
 * @param chr
 * @return 
 */
static int ischr(lex_state_t * state, char chr) {
    return (state->pos[0] == chr);
}

/**
 * Is plus or minus
 * @param c
 * @return 
 */
static int isplusmn(int c) {
    return c == '+' || c == '-';
}

/**
 * Is letter H
 * @param c
 * @return 
 */
static int isH(int c) {
    return c == 'h' || c == 'H';
}

/**
 * Is letter B
 * @param c
 * @return 
 */
static int isB(int c) {
    return c == 'b' || c == 'B';
}

/**
 * Is letter Q
 * @param c
 * @return 
 */
static int isQ(int c) {
    return c == 'q' || c == 'Q';
}

/**
 * Is letter E
 * @param c
 * @return 
 */
static int isE(int c) {
    return c == 'e' || c == 'E';
}

#define SKIP_NONE       0
#define SKIP_OK         1
#define SKIP_INCOMPLETE -1

/* skip characters */
/* 7.4.1 <PROGRAM MESSAGE UNIT SEPARATOR>*/
/* TODO: static int skipProgramMessageUnitSeparator(lex_state_t * state) */

/**
 * Skip all whitespaces
 * @param state
 * @return 
 */
static int skipWs(lex_state_t * state) {
    int someSpace = 0;
    while (!iseos(state) && isws(state->pos[0])) {
        state->pos++;
        someSpace++;
    }

    return someSpace;
}

/* 7.4.2 <PROGRAM DATA SEPARATOR> */
/* static int skipProgramDataSeparator(lex_state_t * state) */

/* 7.5.2 <PROGRAM MESSAGE TERMINATOR> */
/* static int skipProgramMessageTerminator(lex_state_t * state) */

/**
 * Skip decimal digit
 * @param state
 * @return 
 */
static int skipDigit(lex_state_t * state) {
    if (!iseos(state) && isdigit((uint8_t)(state->pos[0]))) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Skip multiple decimal digits
 * @param state
 * @return 
 */
static int skipNumbers(lex_state_t * state) {
    int someNumbers = 0;
    while (!iseos(state) && isdigit((uint8_t)(state->pos[0]))) {
        state->pos++;
        someNumbers++;
    }
    return someNumbers;
}

/**
 * Skip plus or minus
 * @param state
 * @return 
 */
static int skipPlusmn(lex_state_t * state) {
    if (!iseos(state) && isplusmn(state->pos[0])) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Skip any character from 'a'-'Z'
 * @param state
 * @return 
 */
static int skipAlpha(lex_state_t * state) {
    int someLetters = 0;
    while (!iseos(state) && isalpha((uint8_t)(state->pos[0]))) {
        state->pos++;
        someLetters++;
    }
    return someLetters;
}

/**
 * Skip exact character chr or nothing
 * @param state
 * @param chr
 * @return 
 */
static int skipChr(lex_state_t * state, char chr) {
    if (!iseos(state) && ischr(state, chr)) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Skip slash or dot
 * @param state
 * @return 
 */
static int skipSlashDot(lex_state_t * state) {
    if (!iseos(state) && (ischr(state, '/') | ischr(state, '.'))) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Skip star
 * @param state
 * @return 
 */
static int skipStar(lex_state_t * state) {
    if (!iseos(state) && ischr(state, '*')) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Skip colon
 * @param state
 * @return 
 */
static int skipColon(lex_state_t * state) {
    if (!iseos(state) && ischr(state, ':')) {
        state->pos++;
        return SKIP_OK;
    } else {
        return SKIP_NONE;
    }
}

/* 7.6.1.2 <COMMAND PROGRAM HEADER> */

/**
 * Skip program mnemonic [a-z][a-z0-9_]*
 * @param state
 * @return 
 */
static int skipProgramMnemonic(lex_state_t * state) {
    const char * startPos = state->pos;
    if (!iseos(state) && isalpha((uint8_t)(state->pos[0]))) {
        state->pos++;
        while (!iseos(state) && (isalnum((uint8_t)(state->pos[0])) || ischr(state, '_'))) {
            state->pos++;
        }
    }

    if (iseos(state)) {
        return (state->pos - startPos) * SKIP_INCOMPLETE;
    } else {
        return (state->pos - startPos) * SKIP_OK;
    }
}

/* tokens */

/**
 * Detect token white space
 * @param state
 * @param token
 * @return 
 */
int scpiLex_WhiteSpace(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    skipWs(state);

    token->len = state->pos - token->ptr;

    if (token->len > 0) {
        token->type = SCPI_TOKEN_WS;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/* 7.6.1 <COMMAND PROGRAM HEADER> */

/**
 * Skip command program header \*<PROGRAM MNEMONIC>
 * @param state
 * @return 
 */
static int skipCommonProgramHeader(lex_state_t * state) {
    int res;
    if (skipStar(state)) {
        res = skipProgramMnemonic(state);
        if (res == SKIP_NONE && iseos(state)) {
            return SKIP_INCOMPLETE;
        } else if (res <= SKIP_INCOMPLETE) {
            return SKIP_OK;
        } else if (res >= SKIP_OK) {
            return SKIP_OK;
        } else {
            return SKIP_INCOMPLETE;
        }
    }
    return SKIP_NONE;
}

/**
 * Skip compound program header :<PROGRAM MNEMONIC>:<PROGRAM MNEMONIC>...
 * @param state
 * @return 
 */
static int skipCompoundProgramHeader(lex_state_t * state) {
    int res;
    int firstColon = skipColon(state);

    res = skipProgramMnemonic(state);
    if (res >= SKIP_OK) {
        while (skipColon(state)) {
            res = skipProgramMnemonic(state);
            if (res <= SKIP_INCOMPLETE) {
                return SKIP_OK;
            } else if (res == SKIP_NONE) {
                return SKIP_INCOMPLETE;
            }
        }
        return SKIP_OK;
    } else if (res <= SKIP_INCOMPLETE) {
        return SKIP_OK;
    } else if (firstColon) {
        return SKIP_INCOMPLETE;
    } else {
        return SKIP_NONE;
    }
}

/**
 * Detect token command or compound program header
 * @param state
 * @param token
 * @return 
 */
int scpiLex_ProgramHeader(lex_state_t * state, scpi_token_t * token) {
    int res;
    token->ptr = state->pos;
    token->type = SCPI_TOKEN_UNKNOWN;

    res = skipCommonProgramHeader(state);
    if (res >= SKIP_OK) {
        if (skipChr(state, '?') >= SKIP_OK) {
            token->type = SCPI_TOKEN_COMMON_QUERY_PROGRAM_HEADER;
        } else {
            token->type = SCPI_TOKEN_COMMON_PROGRAM_HEADER;
        }
    } else if (res <= SKIP_INCOMPLETE) {
        token->type = SCPI_TOKEN_INCOMPLETE_COMMON_PROGRAM_HEADER;
    } else if (res == SKIP_NONE) {
        res = skipCompoundProgramHeader(state);

        if (res >= SKIP_OK) {
            if (skipChr(state, '?') >= SKIP_OK) {
                token->type = SCPI_TOKEN_COMPOUND_QUERY_PROGRAM_HEADER;
            } else {
                token->type = SCPI_TOKEN_COMPOUND_PROGRAM_HEADER;
            }
        } else if (res <= SKIP_INCOMPLETE) {
            token->type = SCPI_TOKEN_INCOMPLETE_COMPOUND_PROGRAM_HEADER;
        }
    }

    if (token->type != SCPI_TOKEN_UNKNOWN) {
        token->len = state->pos - token->ptr;
    } else {
        token->len = 0;
        state->pos = token->ptr;
    }

    return token->len;
}

/* 7.7.1 <CHARACTER PROGRAM DATA> */

/**
 * Detect token "Character program data"
 * @param state
 * @param token
 * @return 
 */
int scpiLex_CharacterProgramData(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (!iseos(state) && isalpha((uint8_t)(state->pos[0]))) {
        state->pos++;
        while (!iseos(state) && (isalnum((uint8_t)(state->pos[0])) || ischr(state, '_'))) {
            state->pos++;
        }
    }

    token->len = state->pos - token->ptr;
    if (token->len > 0) {
        token->type = SCPI_TOKEN_PROGRAM_MNEMONIC;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/* 7.7.2 <DECIMAL NUMERIC PROGRAM DATA> */
static int skipMantisa(lex_state_t * state) {
    int someNumbers = 0;

    skipPlusmn(state);

    someNumbers += skipNumbers(state);

    if (skipChr(state, '.')) {
        someNumbers += skipNumbers(state);
    }

    return someNumbers;
}

static int skipExponent(lex_state_t * state) {
    int someNumbers = 0;

    if (!iseos(state) && isE(state->pos[0])) {
        state->pos++;

        skipWs(state);

        skipPlusmn(state);

        someNumbers = skipNumbers(state);
    }

    return someNumbers;
}

/**
 * Detect token Decimal number
 * @param state
 * @param token
 * @return 
 */
int scpiLex_DecimalNumericProgramData(lex_state_t * state, scpi_token_t * token) {
    char * rollback;
    token->ptr = state->pos;

    if (skipMantisa(state)) {
        rollback = state->pos;
        skipWs(state);
        if (!skipExponent(state)) {
            state->pos = rollback;
        }
    } else {
        state->pos = token->ptr;
    }

    token->len = state->pos - token->ptr;
    if (token->len > 0) {
        token->type = SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/* 7.7.3 <SUFFIX PROGRAM DATA> */
int scpiLex_SuffixProgramData(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    skipChr(state, '/');

    /* TODO: strict parsing  : SLASH? (ALPHA+ (MINUS? DIGIT)?) ((SLASH | DOT) (ALPHA+ (MINUS? DIGIT)?))* */
    if (skipAlpha(state)) {
        skipChr(state, '-');
        skipDigit(state);

        while (skipSlashDot(state)) {
            skipAlpha(state);
            skipChr(state, '-');
            skipDigit(state);
        }
    }

    token->len = state->pos - token->ptr;
    if ((token->len > 0)) {
        token->type = SCPI_TOKEN_SUFFIX_PROGRAM_DATA;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }

    return token->len;
}

/* 7.7.4 <NONDECIMAL NUMERIC PROGRAM DATA> */
static int skipHexNum(lex_state_t * state) {
    int someNumbers = 0;
    while (!iseos(state) && isxdigit((uint8_t)(state->pos[0]))) {
        state->pos++;
        someNumbers++;
    }
    return someNumbers;
}

static int skipOctNum(lex_state_t * state) {
    int someNumbers = 0;
    while (!iseos(state) && isqdigit(state->pos[0])) {
        state->pos++;
        someNumbers++;
    }
    return someNumbers;
}

static int skipBinNum(lex_state_t * state) {
    int someNumbers = 0;
    while (!iseos(state) && isbdigit(state->pos[0])) {
        state->pos++;
        someNumbers++;
    }
    return someNumbers;
}

/**
 * Detect token nondecimal number
 * @param state
 * @param token
 * @return 
 */
int scpiLex_NondecimalNumericData(lex_state_t * state, scpi_token_t * token) {
    int someNumbers = 0;
    token->ptr = state->pos;
    if (skipChr(state, '#')) {
        if (!iseos(state)) {
            if (isH(state->pos[0])) {
                state->pos++;
                someNumbers = skipHexNum(state);
                token->type = SCPI_TOKEN_HEXNUM;
            } else if (isQ(state->pos[0])) {
                state->pos++;
                someNumbers = skipOctNum(state);
                token->type = SCPI_TOKEN_OCTNUM;
            } else if (isB(state->pos[0])) {
                state->pos++;
                someNumbers = skipBinNum(state);
                token->type = SCPI_TOKEN_BINNUM;
            }
        }
    }

    if (someNumbers) {
        token->ptr += 2; /* ignore number prefix */
        token->len = state->pos - token->ptr;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }
    return token->len > 0 ? token->len + 2 : 0;
}

/* 7.7.5 <STRING PROGRAM DATA> */
static int isascii7bit(int c) {
    return (c >= 0) && (c <= 0x7f);
}

static void skipQuoteProgramData(lex_state_t * state, char quote) {
    while (!iseos(state)) {
        if (isascii7bit(state->pos[0]) && !ischr(state, quote)) {
            state->pos++;
        } else if (ischr(state, quote)) {
            state->pos++;
            if (!iseos(state) && ischr(state, quote)) {
                state->pos++;
            } else {
                state->pos--;
                break;
            }
        } else {
            break;
        }
    }
}

static void skipDoubleQuoteProgramData(lex_state_t * state) {
    skipQuoteProgramData(state, '"');
}

static void skipSingleQuoteProgramData(lex_state_t * state) {
    skipQuoteProgramData(state, '\'');
}

/**
 * Detect token String data
 * @param state
 * @param token
 * @return 
 */
int scpiLex_StringProgramData(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (!iseos(state)) {
        if (ischr(state, '"')) {
            state->pos++;
            token->type = SCPI_TOKEN_DOUBLE_QUOTE_PROGRAM_DATA;
            skipDoubleQuoteProgramData(state);
            if (!iseos(state) && ischr(state, '"')) {
                state->pos++;
                token->len = state->pos - token->ptr;
            } else {
                state->pos = token->ptr;
            }
        } else if (ischr(state, '\'')) {
            state->pos++;
            token->type = SCPI_TOKEN_SINGLE_QUOTE_PROGRAM_DATA;
            skipSingleQuoteProgramData(state);
            if (!iseos(state) && ischr(state, '\'')) {
                state->pos++;
                token->len = state->pos - token->ptr;
            } else {
                state->pos = token->ptr;
            }
        }
    }

    token->len = state->pos - token->ptr;

    if ((token->len > 0)) {
        /* token->ptr++;
         * token->len -= 2; */
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }

    return token->len > 0 ? token->len : 0;
}

/* 7.7.6 <ARBITRARY BLOCK PROGRAM DATA> */
static int isNonzeroDigit(int c) {
    return isdigit(c) && (c != '0');
}

/**
 * Detect token Block Data
 * @param state
 * @param token
 * @return 
 */
int scpiLex_ArbitraryBlockProgramData(lex_state_t * state, scpi_token_t * token) {
    int i;
    int arbitraryBlockLength = 0;
    const char * ptr = state->pos;
    int validData = -1;
    token->ptr = state->pos;

    if (skipChr(state, '#')) {
        if (!iseos(state) && isNonzeroDigit(state->pos[0])) {
            /* Get number of digits */
            i = state->pos[0] - '0';
            state->pos++;

            for (; i > 0; i--) {
                if (!iseos(state) && isdigit((uint8_t)(state->pos[0]))) {
                    arbitraryBlockLength *= 10;
                    arbitraryBlockLength += (state->pos[0] - '0');
                    state->pos++;
                } else {
                    break;
                }
            }

            if (i == 0) {
                state->pos += arbitraryBlockLength;
                if ((state->buffer + state->len) >= (state->pos)) {
                    token->ptr = state->pos - arbitraryBlockLength;
                    token->len = arbitraryBlockLength;
                    validData = 1;
                } else {
                    validData = 0;
                }
            } else if (iseos(state)) {
                validData = 0;
            }
        } else if (iseos(state)) {
            validData = 0;
        }
    }

    if (validData == 1) {
        /* valid */
        token->type = SCPI_TOKEN_ARBITRARY_BLOCK_PROGRAM_DATA;
    } else if (validData == 0) {
        /* incomplete */
        token->type = SCPI_TOKEN_UNKNOWN;
        token->len = 0;
        state->pos = state->buffer + state->len;
    } else {
        /* invalid */
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }

    return token->len + (token->ptr - ptr);
}

/* 7.7.7 <EXPRESSION PROGRAM DATA> */
static int isProgramExpression(int c) {
    if ((c >= 0x20) && (c <= 0x7e)) {
        if ((c != '"')
                && (c != '#')
                && (c != '\'')
                && (c != '(')
                && (c != ')')
                && (c != ';')) {
            return 1;
        }
    }

    return 0;
}

static void skipProgramExpression(lex_state_t * state) {
    while (!iseos(state) && isProgramExpression(state->pos[0])) {
        state->pos++;
    }
}

/* TODO: 7.7.7.2-2 recursive - any program data */

/**
 * Detect token Expression
 * @param state
 * @param token
 * @return 
 */
int scpiLex_ProgramExpression(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (!iseos(state) && ischr(state, '(')) {
        state->pos++;
        skipProgramExpression(state);

        if (!iseos(state) && ischr(state, ')')) {
            state->pos++;
            token->len = state->pos - token->ptr;
        } else {
            token->len = 0;
        }
    }

    if ((token->len > 0)) {
        token->type = SCPI_TOKEN_PROGRAM_EXPRESSION;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }

    return token->len;
}

/**
 * Detect token comma
 * @param state
 * @param token
 * @return 
 */
int scpiLex_Comma(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (skipChr(state, ',')) {
        token->len = 1;
        token->type = SCPI_TOKEN_COMMA;
    } else {
        token->len = 0;
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/**
 * Detect token semicolon
 * @param state
 * @param token
 * @return 
 */
int scpiLex_Semicolon(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (skipChr(state, ';')) {
        token->len = 1;
        token->type = SCPI_TOKEN_SEMICOLON;
    } else {
        token->len = 0;
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/**
 * Detect token colon
 * @param state
 * @param token
 * @return 
 */
int scpiLex_Colon(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    if (skipChr(state, ':')) {
        token->len = 1;
        token->type = SCPI_TOKEN_COLON;
    } else {
        token->len = 0;
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/**
 * Detect specified character
 * @param state
 * @param token
 * @return 
 */
int scpiLex_SpecificCharacter(lex_state_t * state, scpi_token_t * token, char chr) {
    token->ptr = state->pos;

    if (skipChr(state, chr)) {
        token->len = 1;
        token->type = SCPI_TOKEN_SPECIFIC_CHARACTER;
    } else {
        token->len = 0;
        token->type = SCPI_TOKEN_UNKNOWN;
    }

    return token->len;
}

/**
 * Detect token New line
 * @param state
 * @param token
 * @return 
 */
int scpiLex_NewLine(lex_state_t * state, scpi_token_t * token) {
    token->ptr = state->pos;

    skipChr(state, '\r');
    skipChr(state, '\n');

    token->len = state->pos - token->ptr;

    if ((token->len > 0)) {
        token->type = SCPI_TOKEN_NL;
    } else {
        token->type = SCPI_TOKEN_UNKNOWN;
        state->pos = token->ptr;
        token->len = 0;
    }

    return token->len;
}
