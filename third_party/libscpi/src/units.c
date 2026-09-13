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
 * @file   scpi_units.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  SCPI units
 *
 *
 */

#include <string.h>
#include "scpi/parser.h"
#include "scpi/units.h"
#include "utils_private.h"
#include "scpi/utils.h"
#include "scpi/error.h"
#include "lexer_private.h"


/*
 * multipliers IEEE 488.2-1992 tab 7-2
 * 1E18         EX
 * 1E15         PE
 * 1E12         T
 * 1E9          G
 * 1E6          MA (use M for OHM and HZ)
 * 1E3          K
 * 1E-3         M (disaalowed for OHM and HZ)
 * 1E-6         U
 * 1E-9         N
 * 1E-12        P
 * 1E-15        F
 * 1E-18        A
 */

/*
 * units definition IEEE 488.2-1992 tab 7-1
 */
const scpi_unit_def_t scpi_units_def[] = {
#if USE_UNITS_PARTICLES
    /* Absorbet dose */
    {/* name */ "GY", /* unit */ SCPI_UNIT_GRAY, /* mult */ 1},

    /* Activity of radionuclide */
    {/* name */ "BQ", /* unit */ SCPI_UNIT_BECQUEREL, /* mult */ 1},

    /* Amount of substance */
    {/* name */ "MOL", /* unit */ SCPI_UNIT_MOLE, /* mult */ 1},

    /* Dose equivalent */
    {/* name */ "NSV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1e-9},
    {/* name */ "USV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1e-6},
    {/* name */ "MSV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1e-3},
    {/* name */ "SV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1},
    {/* name */ "KSV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1e3},
    {/* name */ "MASV", /* unit */ SCPI_UNIT_SIEVERT, /* mult */ 1e6},

    /* Energy */
    {/* name */ "EV", /* unit */ SCPI_UNIT_ELECTRONVOLT, /* mult */ 1},
    {/* name */ "KEV", /* unit */ SCPI_UNIT_ELECTRONVOLT, /* mult */ 1e3},
    {/* name */ "MAEV", /* unit */ SCPI_UNIT_ELECTRONVOLT, /* mult */ 1e6},
    {/* name */ "GEV", /* unit */ SCPI_UNIT_ELECTRONVOLT, /* mult */ 1e9},
    {/* name */ "TEV", /* unit */ SCPI_UNIT_ELECTRONVOLT, /* mult */ 1e12},

    /* Mass */
    {/* name */ "U", /* unit */ SCPI_UNIT_ATOMIC_MASS, /* mult */ 1},
#endif /* USE_UNITS_PARTICLES */

#if USE_UNITS_ANGLE
    /* Angle */
    {/* name */ "DEG", /* unit */ SCPI_UNIT_DEGREE, /* mult */ 1},
    {/* name */ "GON", /* unit */ SCPI_UNIT_GRADE, /* mult */ 1},
    {/* name */ "MNT", /* unit */ SCPI_UNIT_DEGREE, /* mult */ 1. / 60.},
    {/* name */ "RAD", /* unit */ SCPI_UNIT_RADIAN, /* mult */ 1},
    {/* name */ "SEC", /* unit */ SCPI_UNIT_DEGREE, /* mult */ 1. / 3600.},
    {/* name */ "REV", /* unit */ SCPI_UNIT_REVOLUTION, /* mult */ 1},
    {/* name */ "RS", /* unit */ SCPI_UNIT_STERADIAN, /* mult */ 1},
#endif /* USE_UNITS_ANGLE */

#if USE_UNITS_ELECTRIC
    /* Electric - capacitance */
    {/* name */ "PF", /* unit */ SCPI_UNIT_FARAD, /* mult */ 1e-12},
    {/* name */ "NF", /* unit */ SCPI_UNIT_FARAD, /* mult */ 1e-9},
    {/* name */ "UF", /* unit */ SCPI_UNIT_FARAD, /* mult */ 1e-6},
    {/* name */ "MF", /* unit */ SCPI_UNIT_FARAD, /* mult */ 1e-3},
    {/* name */ "F", /* unit */ SCPI_UNIT_FARAD, /* mult */ 1},

    /* Electric - current */
    {/* name */ "UA", /* unit */ SCPI_UNIT_AMPER, /* mult */ 1e-6},
    {/* name */ "MA", /* unit */ SCPI_UNIT_AMPER, /* mult */ 1e-3},
    {/* name */ "A", /* unit */ SCPI_UNIT_AMPER, /* mult */ 1},
    {/* name */ "KA", /* unit */ SCPI_UNIT_AMPER, /* mult */ 1e3},

    /* Electric - potential */
    {/* name */ "UV", /* unit */ SCPI_UNIT_VOLT, /* mult */ 1e-6},
    {/* name */ "MV", /* unit */ SCPI_UNIT_VOLT, /* mult */ 1e-3},
    {/* name */ "V", /* unit */ SCPI_UNIT_VOLT, /* mult */ 1},
    {/* name */ "KV", /* unit */ SCPI_UNIT_VOLT, /* mult */ 1e3},

    /* Electric - resistance */
    {/* name */ "OHM", /* unit */ SCPI_UNIT_OHM, /* mult */ 1},
    {/* name */ "KOHM", /* unit */ SCPI_UNIT_OHM, /* mult */ 1e3},
    {/* name */ "MOHM", /* unit */ SCPI_UNIT_OHM, /* mult */ 1e6},

    /* Inductance */
    {/* name */ "UH", /* unit */ SCPI_UNIT_HENRY, /* mult */ 1e-6},
    {/* name */ "MH", /* unit */ SCPI_UNIT_HENRY, /* mult */ 1e-3},
    {/* name */ "H", /* unit */ SCPI_UNIT_HENRY, /* mult */ 1},
#endif /* USE_UNITS_ELECTRIC */

#if USE_UNITS_ELECTRIC_CHARGE_CONDUCTANCE
    /* Electric - charge */
    {/* name */ "C", /* unit */ SCPI_UNIT_COULOMB, /* mult */ 1},

    /* Electric - conductance */
    {/* name */ "USIE", /* unit */ SCPI_UNIT_SIEMENS, /* mult */ 1e-6},
    {/* name */ "MSIE", /* unit */ SCPI_UNIT_SIEMENS, /* mult */ 1e-3},
    {/* name */ "SIE", /* unit */ SCPI_UNIT_SIEMENS, /* mult */ 1},
#endif /* USE_UNITS_ELECTRIC_CHARGE_CONDUCTANCE */

#if USE_UNITS_ENERGY_FORCE_MASS
    /* Energy */
    {/* name */ "J", /* unit */ SCPI_UNIT_JOULE, /* mult */ 1},
    {/* name */ "KJ", /* unit */ SCPI_UNIT_JOULE, /* mult */ 1e3},
    {/* name */ "MAJ", /* unit */ SCPI_UNIT_JOULE, /* mult */ 1e6},

    /* Force */
    {/* name */ "N", /* unit */ SCPI_UNIT_NEWTON, /* mult */ 1},
    {/* name */ "KN", /* unit */ SCPI_UNIT_NEWTON, /* mult */ 1e3},

    /* Pressure */
    {/* name */ "ATM", /* unit */ SCPI_UNIT_ATMOSPHERE, /* mult */ 1},
    {/* name */ "INHG", /* unit */ SCPI_UNIT_INCH_OF_MERCURY, /* mult */ 1},
    {/* name */ "MMHG", /* unit */ SCPI_UNIT_MM_OF_MERCURY, /* mult */ 1},

    {/* name */ "TORR", /* unit */ SCPI_UNIT_TORT, /* mult */ 1},
    {/* name */ "BAR", /* unit */ SCPI_UNIT_BAR, /* mult */ 1},

    {/* name */ "PAL", /* unit */ SCPI_UNIT_PASCAL, /* mult */ 1},
    {/* name */ "KPAL", /* unit */ SCPI_UNIT_PASCAL, /* mult */ 1e3},
    {/* name */ "MAPAL", /* unit */ SCPI_UNIT_PASCAL, /* mult */ 1e6},

    /* Viscosity kinematic */
    {/* name */ "ST", /* unit */ SCPI_UNIT_STROKES, /* mult */ 1},

    /* Viscosity dynamic */
    {/* name */ "P", /* unit */ SCPI_UNIT_POISE, /* mult */ 1},

    /* Viscosity dynamic */
    {/* name */ "L", /* unit */ SCPI_UNIT_LITER, /* mult */ 1},

    /* Mass */
    {/* name */ "MG", /* unit */ SCPI_UNIT_KILOGRAM, /* mult */ 1e-6},
    {/* name */ "G", /* unit */ SCPI_UNIT_KILOGRAM, /* mult */ 1e-3},
    {/* name */ "KG", /* unit */ SCPI_UNIT_KILOGRAM, /* mult */ 1},
    {/* name */ "TNE", /* unit */ SCPI_UNIT_KILOGRAM, /* mult */ 1000},
#endif /* USE_UNITS_ENERGY_FORCE_MASS */

#if USE_UNITS_FREQUENCY
    /* Frequency */
    {/* name */ "HZ", /* unit */ SCPI_UNIT_HERTZ, /* mult */ 1},
    {/* name */ "KHZ", /* unit */ SCPI_UNIT_HERTZ, /* mult */ 1e3},
    {/* name */ "MHZ", /* unit */ SCPI_UNIT_HERTZ, /* mult */ 1e6},
    {/* name */ "GHZ", /* unit */ SCPI_UNIT_HERTZ, /* mult */ 1e9},
#endif /* USE_UNITS_FREQUENCY */

#if USE_UNITS_DISTANCE
    /* Length */
    {/* name */ "ASU", /* unit */ SCPI_UNIT_ASTRONOMIC_UNIT, /* mult */ 1},
    {/* name */ "PRS", /* unit */ SCPI_UNIT_PARSEC, /* mult */ 1},
#if USE_UNITS_IMPERIAL
    {/* name */ "IN", /* unit */ SCPI_UNIT_INCH, /* mult */ 1},
    {/* name */ "FT", /* unit */ SCPI_UNIT_FOOT, /* mult */ 1},
    {/* name */ "MI", /* unit */ SCPI_UNIT_MILE, /* mult */ 1},
    {/* name */ "NAMI", /* unit */ SCPI_UNIT_NAUTICAL_MILE, /* mult */ 1},
#endif /* USE_UNITS_IMPERIAL */

    {/* name */ "NM", /* unit */ SCPI_UNIT_METER, /* mult */ 1e-9},
    {/* name */ "UM", /* unit */ SCPI_UNIT_METER, /* mult */ 1e-6},
    {/* name */ "MM", /* unit */ SCPI_UNIT_METER, /* mult */ 1e-3},
    {/* name */ "M", /* unit */ SCPI_UNIT_METER, /* mult */ 1},
    {/* name */ "KM", /* unit */ SCPI_UNIT_METER, /* mult */ 1e3},
#endif /* USE_UNITS_DISTANCE */

#if USE_UNITS_LIGHT
    /* Illuminance */
    {/* name */ "LX", /* unit */ SCPI_UNIT_LUX, /* mult */ 1},

    /* Luminous flux */
    {/* name */ "LM", /* unit */ SCPI_UNIT_LUMEN, /* mult */ 1},

    /* Luminous intensity */
    {/* name */ "CD", /* unit */ SCPI_UNIT_CANDELA, /* mult */ 1},
#endif /* USE_UNITS_LIGHT */

#if USE_UNITS_MAGNETIC
    /* Magnetic flux */
    {/* name */ "WB", /* unit */ SCPI_UNIT_WEBER, /* mult */ 1},

    /* Magnetic induction */
    {/* name */ "NT", /* unit */ SCPI_UNIT_TESLA, /* mult */ 1e-9},
    {/* name */ "UT", /* unit */ SCPI_UNIT_TESLA, /* mult */ 1e-6},
    {/* name */ "MT", /* unit */ SCPI_UNIT_TESLA, /* mult */ 1e-3},
    {/* name */ "T", /* unit */ SCPI_UNIT_TESLA, /* mult */ 1},
#endif /* USE_UNITS_MAGNETIC */

#if USE_UNITS_POWER
    /* Power */
    {/* name */ "W", /* unit */ SCPI_UNIT_WATT, /* mult */ 1},
    {/* name */ "DBM", /* unit */ SCPI_UNIT_DBM, /* mult */ 1},
    {/* name */ "DBMW", /* unit */ SCPI_UNIT_DBM, /* mult */ 1},
#endif /* USE_UNITS_POWER  */

#if USE_UNITS_RATIO
    /* Ratio */
    {/* name */ "DB", /* unit */ SCPI_UNIT_DECIBEL, /* mult */ 1},
    {/* name */ "PCT", /* unit */ SCPI_UNIT_UNITLESS, /* mult */ 1e-2},
    {/* name */ "PPM", /* unit */ SCPI_UNIT_UNITLESS, /* mult */ 1e-6},
#endif /* USE_UNITS_RATIO */

#if USE_UNITS_TEMPERATURE
    /* Temperature */
    {/* name */ "CEL", /* unit */ SCPI_UNIT_CELSIUS, /* mult */ 1},
#if USE_UNITS_IMPERIAL
    {/* name */ "FAR", /* unit */ SCPI_UNIT_FAHRENHEIT, /* mult */ 1},
#endif /* USE_UNITS_IMPERIAL */
    {/* name */ "K", /* unit */ SCPI_UNIT_KELVIN, /* mult */ 1},
#endif /* USE_UNITS_TEMPERATURE */

#if USE_UNITS_TIME
    /* Time */
    {/* name */ "PS", /* unit */ SCPI_UNIT_SECOND, /* mult */ 1e-12},
    {/* name */ "NS", /* unit */ SCPI_UNIT_SECOND, /* mult */ 1e-9},
    {/* name */ "US", /* unit */ SCPI_UNIT_SECOND, /* mult */ 1e-6},
    {/* name */ "MS", /* unit */ SCPI_UNIT_SECOND, /* mult */ 1e-3},
    {/* name */ "S", /* unit */ SCPI_UNIT_SECOND, /* mult */ 1},
    {/* name */ "MIN", /* unit */ SCPI_UNIT_SECOND, /* mult */ 60},
    {/* name */ "HR", /* unit */ SCPI_UNIT_SECOND, /* mult */ 3600},
    {/* name */ "D", /* unit */ SCPI_UNIT_DAY, /* mult */ 1},
    {/* name */ "ANN", /* unit */ SCPI_UNIT_YEAR, /* mult */ 1},
#endif /* USE_UNITS_TIME */

    SCPI_UNITS_LIST_END,
};

/*
 * Special number values definition
 */
const scpi_choice_def_t scpi_special_numbers_def[] = {
    {/* name */ "MINimum", /* type */ SCPI_NUM_MIN},
    {/* name */ "MAXimum", /* type */ SCPI_NUM_MAX},
    {/* name */ "DEFault", /* type */ SCPI_NUM_DEF},
    {/* name */ "UP", /* type */ SCPI_NUM_UP},
    {/* name */ "DOWN", /* type */ SCPI_NUM_DOWN},
    {/* name */ "NAN", /* type */ SCPI_NUM_NAN},
    {/* name */ "INFinity", /* type */ SCPI_NUM_INF},
    {/* name */ "NINF", /* type */ SCPI_NUM_NINF},
    {/* name */ "AUTO", /* type */ SCPI_NUM_AUTO},
    SCPI_CHOICE_LIST_END,
};

/**
 * Convert string describing unit to its representation
 * @param units units patterns
 * @param unit text representation of unknown unit
 * @param len length of text representation
 * @return pointer of related unit definition or NULL
 */
static const scpi_unit_def_t * translateUnit(const scpi_unit_def_t * units, const char * unit, size_t len) {
    int i;

    if (units == NULL) {
        return NULL;
    }

    for (i = 0; units[i].name != NULL; i++) {
        if (compareStr(unit, len, units[i].name, strlen(units[i].name))) {
            return &units[i];
        }
    }

    return NULL;
}

/**
 * Convert unit definition to string
 * @param units units definitions (patterns)
 * @param unit type of unit
 * @return string representation of unit
 */
static const char * translateUnitInverse(const scpi_unit_def_t * units, const scpi_unit_t unit) {
    int i;

    if (units == NULL) {
        return NULL;
    }

    for (i = 0; units[i].name != NULL; i++) {
        if ((units[i].unit == unit) && (units[i].mult == 1)) {
            return units[i].name;
        }
    }

    return NULL;
}

/**
 * Transform number to base units
 * @param context
 * @param unit text representation of unit
 * @param len length of text representation
 * @param value preparsed numeric value
 * @return TRUE if value parameter was converted to base units
 */
static scpi_bool_t transformNumber(scpi_t * context, const char * unit, size_t len, scpi_number_t * value) {
    size_t s;
    const scpi_unit_def_t * unitDef;
    s = skipWhitespace(unit, len);

    if (s == len) {
        value->unit = SCPI_UNIT_NONE;
        return TRUE;
    }

    unitDef = translateUnit(context->units, unit + s, len - s);

    if (unitDef == NULL) {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
        return FALSE;
    }

    value->content.value *= unitDef->mult;
    value->unit = unitDef->unit;

    return TRUE;
}

/**
 * Parse parameter as number, number with unit or special value (min, max, default, ...)
 * @param context
 * @param value return value
 * @param mandatory if the parameter is mandatory
 * @return
 */
scpi_bool_t SCPI_ParamNumber(scpi_t * context, const scpi_choice_def_t * special, scpi_number_t * value, scpi_bool_t mandatory) {
    scpi_token_t token;
    lex_state_t state;
    scpi_parameter_t param;
    scpi_bool_t result;
    int32_t tag;

    if (!value) {
        SCPI_ErrorPush(context, SCPI_ERROR_SYSTEM_ERROR);
        return FALSE;
    }

    result = SCPI_Parameter(context, &param, mandatory);

    if (!result) {
        return result;
    }

    state.buffer = param.ptr;
    state.pos = state.buffer;
    state.len = param.len;

    switch (param.type) {
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA:
        case SCPI_TOKEN_HEXNUM:
        case SCPI_TOKEN_OCTNUM:
        case SCPI_TOKEN_BINNUM:
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX:
        case SCPI_TOKEN_PROGRAM_MNEMONIC:
            value->unit = SCPI_UNIT_NONE;
            value->special = FALSE;
            result = TRUE;
            break;
        default:
            break;
    }

    switch (param.type) {
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA:
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX:
        case SCPI_TOKEN_PROGRAM_MNEMONIC:
            value->base = 10;
            break;
        case SCPI_TOKEN_BINNUM:
            value->base = 2;
            break;
        case SCPI_TOKEN_HEXNUM:
            value->base = 16;
            break;
        case SCPI_TOKEN_OCTNUM:
            value->base = 8;
            break;
        default:
            break;
    }

    switch (param.type) {
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA:
            SCPI_ParamToDouble(context, &param, &(value->content.value));
            break;
        case SCPI_TOKEN_HEXNUM:
            SCPI_ParamToDouble(context, &param, &(value->content.value));
            break;
        case SCPI_TOKEN_OCTNUM:
            SCPI_ParamToDouble(context, &param, &(value->content.value));
            break;
        case SCPI_TOKEN_BINNUM:
            SCPI_ParamToDouble(context, &param, &(value->content.value));
            break;
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX:
            scpiLex_DecimalNumericProgramData(&state, &token);
            scpiLex_WhiteSpace(&state, &token);
            scpiLex_SuffixProgramData(&state, &token);

            SCPI_ParamToDouble(context, &param, &(value->content.value));

            result = transformNumber(context, token.ptr, token.len, value);
            break;
        case SCPI_TOKEN_PROGRAM_MNEMONIC:
            scpiLex_WhiteSpace(&state, &token);
            scpiLex_CharacterProgramData(&state, &token);

            /* convert string to special number type */
            result = SCPI_ParamToChoice(context, &token, special, &tag);

            value->special = TRUE;
            value->content.tag = tag;

            break;
        default:
            result = FALSE;
    }

    return result;
}

/**
 * Convert scpi_number_t to string
 * @param context
 * @param value number value
 * @param str target string
 * @param len max length of string including null-character termination
 * @return number of chars written to string
 */
size_t SCPI_NumberToStr(scpi_t * context, const scpi_choice_def_t * special, scpi_number_t * value, char * str, size_t len) {
    const char * type;
    const char * unit;
    size_t result;

    if (!value || !str || len==0) {
        return 0;
    }

    if (value->special) {
        if (SCPI_ChoiceToName(special, value->content.tag, &type)) {
            strncpy(str, type, len);
            result = SCPIDEFINE_strnlen(str, len - 1);
            str[result] = '\0';
            return result;
        } else {
            str[0] = '\0';
            return 0;
        }
    }

    result = SCPI_DoubleToStr(value->content.value, str, len);

    if (result + 1 < len) {
        unit = translateUnitInverse(context->units, value->unit);

        if (unit) {
            strncat(str, " ", len - result);
            if (result + 2 < len) {
                strncat(str, unit, len - result - 1);
            }
            result = strlen(str);
        }
    }

    return result;
}
