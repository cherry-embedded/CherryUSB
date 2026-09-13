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
 * @file   scpi_types.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  SCPI data types
 *
 *
 */

#ifndef SCPI_TYPES_H
#define SCPI_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "scpi/config.h"

#if HAVE_STDBOOL
#include <stdbool.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#if !HAVE_STDBOOL
    typedef unsigned char bool;
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

    /* basic data types */
    typedef bool scpi_bool_t;
    /* typedef enum { FALSE = 0, TRUE } scpi_bool_t; */

    /* IEEE 488.2 registers */
    enum _scpi_reg_name_t {
        SCPI_REG_STB = 0, /* Status Byte */
        SCPI_REG_SRE, /* Service Request Enable Register */
        SCPI_REG_ESR, /* Standard Event Status Register (ESR, SESR) */
        SCPI_REG_ESE, /* Event Status Enable Register */
        SCPI_REG_OPER, /* OPERation Status Register */
        SCPI_REG_OPERE, /* OPERation Status Enable Register */
        SCPI_REG_OPERC, /* OPERation Status Condition Register */
        SCPI_REG_QUES, /* QUEStionable status register */
        SCPI_REG_QUESE, /* QUEStionable status Enable Register */
        SCPI_REG_QUESC, /* QUEStionable status Condition Register */

#if USE_CUSTOM_REGISTERS
#ifndef USER_REGISTERS
#error "No user registers defined"
#else
        USER_REGISTERS
#endif
#endif

        /* number of registers */
        SCPI_REG_COUNT,
        /* last definition - a value for no register */
        SCPI_REG_NONE
    };
    typedef enum _scpi_reg_name_t scpi_reg_name_t;

    enum _scpi_ctrl_name_t {
        SCPI_CTRL_SRQ = 1, /* service request */
        SCPI_CTRL_GTL, /* Go to local */
        SCPI_CTRL_SDC, /* Selected device clear */
        SCPI_CTRL_PPC, /* Parallel poll configure */
        SCPI_CTRL_GET, /* Group execute trigger */
        SCPI_CTRL_TCT, /* Take control */
        SCPI_CTRL_LLO, /* Device clear */
        SCPI_CTRL_DCL, /* Local lockout */
        SCPI_CTRL_PPU, /* Parallel poll unconfigure */
        SCPI_CTRL_SPE, /* Serial poll enable */
        SCPI_CTRL_SPD, /* Serial poll disable */
        SCPI_CTRL_MLA, /* My local address */
        SCPI_CTRL_UNL, /* Unlisten */
        SCPI_CTRL_MTA, /* My talk address */
        SCPI_CTRL_UNT, /* Untalk */
        SCPI_CTRL_MSA /* My secondary address */
    };
    typedef enum _scpi_ctrl_name_t scpi_ctrl_name_t;

    typedef uint16_t scpi_reg_val_t;

    enum _scpi_reg_class_t {
        SCPI_REG_CLASS_STB = 0,
        SCPI_REG_CLASS_SRE,
        SCPI_REG_CLASS_EVEN,
        SCPI_REG_CLASS_ENAB,
        SCPI_REG_CLASS_COND,
        SCPI_REG_CLASS_NTR,
        SCPI_REG_CLASS_PTR,
    };
    typedef enum _scpi_reg_class_t scpi_reg_class_t;

    enum _scpi_reg_group_t {
        SCPI_REG_GROUP_STB = 0,
        SCPI_REG_GROUP_ESR,
        SCPI_REG_GROUP_OPER,
        SCPI_REG_GROUP_QUES,

#if USE_CUSTOM_REGISTERS
#ifndef USER_REGISTER_GROUPS
#error "No user register groups defined"
#else
        USER_REGISTER_GROUPS
#endif
#endif

        /* last definition - number of register groups */
        SCPI_REG_GROUP_COUNT
    };
    typedef enum _scpi_reg_group_t scpi_reg_group_t;

    struct _scpi_reg_info_t {
        scpi_reg_class_t type;
        scpi_reg_group_t group;
    };
    typedef struct _scpi_reg_info_t scpi_reg_info_t;

    struct _scpi_reg_group_info_t {
        scpi_reg_name_t event;
        scpi_reg_name_t enable;
        scpi_reg_name_t condition;
        scpi_reg_name_t ptfilt;
        scpi_reg_name_t ntfilt;
        scpi_reg_name_t parent_reg;
        scpi_reg_val_t parent_bit;
    };
    typedef struct _scpi_reg_group_info_t scpi_reg_group_info_t;

    /* scpi commands */
    enum _scpi_result_t {
        SCPI_RES_OK = 1,
        SCPI_RES_ERR = -1
    };
    typedef enum _scpi_result_t scpi_result_t;

    typedef struct _scpi_command_t scpi_command_t;

#if USE_COMMAND_TAGS
	#define SCPI_CMD_LIST_END       {NULL, NULL, 0}
#else
	#define SCPI_CMD_LIST_END       {NULL, NULL}
#endif


    /* scpi interface */
    typedef struct _scpi_t scpi_t;
    typedef struct _scpi_interface_t scpi_interface_t;

    struct _scpi_buffer_t {
        size_t length;
        size_t position;
        char * data;
    };
    typedef struct _scpi_buffer_t scpi_buffer_t;

    struct _scpi_const_buffer_t {
        size_t length;
        size_t position;
        const char * data;
    };
    typedef struct _scpi_const_buffer_t scpi_const_buffer_t;

    typedef size_t(*scpi_write_t)(scpi_t * context, const char * data, size_t len);
    typedef scpi_result_t(*scpi_write_control_t)(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
    typedef int (*scpi_error_callback_t)(scpi_t * context, int_fast16_t error);

    /* scpi lexer */
    enum _scpi_token_type_t {
        SCPI_TOKEN_COMMA,
        SCPI_TOKEN_SEMICOLON,
        SCPI_TOKEN_COLON,
        SCPI_TOKEN_SPECIFIC_CHARACTER,
        SCPI_TOKEN_QUESTION,
        SCPI_TOKEN_NL,
        SCPI_TOKEN_HEXNUM,
        SCPI_TOKEN_OCTNUM,
        SCPI_TOKEN_BINNUM,
        SCPI_TOKEN_PROGRAM_MNEMONIC,
        SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA,
        SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX,
        SCPI_TOKEN_SUFFIX_PROGRAM_DATA,
        SCPI_TOKEN_ARBITRARY_BLOCK_PROGRAM_DATA,
        SCPI_TOKEN_SINGLE_QUOTE_PROGRAM_DATA,
        SCPI_TOKEN_DOUBLE_QUOTE_PROGRAM_DATA,
        SCPI_TOKEN_PROGRAM_EXPRESSION,
        SCPI_TOKEN_COMPOUND_PROGRAM_HEADER,
        SCPI_TOKEN_INCOMPLETE_COMPOUND_PROGRAM_HEADER,
        SCPI_TOKEN_COMMON_PROGRAM_HEADER,
        SCPI_TOKEN_INCOMPLETE_COMMON_PROGRAM_HEADER,
        SCPI_TOKEN_COMPOUND_QUERY_PROGRAM_HEADER,
        SCPI_TOKEN_COMMON_QUERY_PROGRAM_HEADER,
        SCPI_TOKEN_WS,
        SCPI_TOKEN_ALL_PROGRAM_DATA,
        SCPI_TOKEN_INVALID,
        SCPI_TOKEN_UNKNOWN,
    };
    typedef enum _scpi_token_type_t scpi_token_type_t;

    struct _scpi_token_t {
        scpi_token_type_t type;
        char * ptr;
        int len;
    };
    typedef struct _scpi_token_t scpi_token_t;

    struct _lex_state_t {
        char * buffer;
        char * pos;
        int len;
    };
    typedef struct _lex_state_t lex_state_t;

    /* scpi parser */
    enum _message_termination_t {
        SCPI_MESSAGE_TERMINATION_NONE,
        SCPI_MESSAGE_TERMINATION_NL,
        SCPI_MESSAGE_TERMINATION_SEMICOLON,
    };
    typedef enum _message_termination_t message_termination_t;

    struct _scpi_parser_state_t {
        scpi_token_t programHeader;
        scpi_token_t programData;
        int numberOfParameters;
        message_termination_t termination;
    };
    typedef struct _scpi_parser_state_t scpi_parser_state_t;

    typedef scpi_result_t(*scpi_command_callback_t)(scpi_t *);

    struct _scpi_error_info_heap_t {
        size_t wr;
        /* size_t rd; */
        size_t count;
        size_t size;
        char * data;
    };
    typedef struct _scpi_error_info_heap_t scpi_error_info_heap_t;

    struct _scpi_error_t {
        int16_t error_code;
#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION
        char * device_dependent_info;
#endif
    };
    typedef struct _scpi_error_t scpi_error_t;

    struct _scpi_fifo_t {
        int16_t wr;
        int16_t rd;
        int16_t count;
        int16_t size;
        scpi_error_t * data;
    };
    typedef struct _scpi_fifo_t scpi_fifo_t;

    /* scpi units */
    enum _scpi_unit_t {
        SCPI_UNIT_NONE,
        SCPI_UNIT_VOLT,
        SCPI_UNIT_AMPER,
        SCPI_UNIT_OHM,
        SCPI_UNIT_HERTZ,
        SCPI_UNIT_CELSIUS,
        SCPI_UNIT_SECOND,
        SCPI_UNIT_METER,
        SCPI_UNIT_GRAY,
        SCPI_UNIT_BECQUEREL,
        SCPI_UNIT_MOLE,
        SCPI_UNIT_DEGREE,
        SCPI_UNIT_GRADE,
        SCPI_UNIT_RADIAN,
        SCPI_UNIT_REVOLUTION,
        SCPI_UNIT_STERADIAN,
        SCPI_UNIT_SIEVERT,
        SCPI_UNIT_FARAD,
        SCPI_UNIT_COULOMB,
        SCPI_UNIT_SIEMENS,
        SCPI_UNIT_ELECTRONVOLT,
        SCPI_UNIT_JOULE,
        SCPI_UNIT_NEWTON,
        SCPI_UNIT_LUX,
        SCPI_UNIT_HENRY,
        SCPI_UNIT_ASTRONOMIC_UNIT,
        SCPI_UNIT_INCH,
        SCPI_UNIT_FOOT,
        SCPI_UNIT_PARSEC,
        SCPI_UNIT_MILE,
        SCPI_UNIT_NAUTICAL_MILE,
        SCPI_UNIT_LUMEN,
        SCPI_UNIT_CANDELA,
        SCPI_UNIT_WEBER,
        SCPI_UNIT_TESLA,
        SCPI_UNIT_ATOMIC_MASS,
        SCPI_UNIT_KILOGRAM,
        SCPI_UNIT_WATT,
        SCPI_UNIT_DBM,
        SCPI_UNIT_ATMOSPHERE,
        SCPI_UNIT_INCH_OF_MERCURY,
        SCPI_UNIT_MM_OF_MERCURY,
        SCPI_UNIT_PASCAL,
        SCPI_UNIT_TORT,
        SCPI_UNIT_BAR,
        SCPI_UNIT_DECIBEL,
        SCPI_UNIT_UNITLESS,
        SCPI_UNIT_FAHRENHEIT,
        SCPI_UNIT_KELVIN,
        SCPI_UNIT_DAY,
        SCPI_UNIT_YEAR,
        SCPI_UNIT_STROKES,
        SCPI_UNIT_POISE,
        SCPI_UNIT_LITER
    };
    typedef enum _scpi_unit_t scpi_unit_t;

    struct _scpi_unit_def_t {
        const char * name;
        scpi_unit_t unit;
        double mult;
    };
#define SCPI_UNITS_LIST_END       {NULL, SCPI_UNIT_NONE, 0}
    typedef struct _scpi_unit_def_t scpi_unit_def_t;

    enum _scpi_special_number_t {
        SCPI_NUM_NUMBER,
        SCPI_NUM_MIN,
        SCPI_NUM_MAX,
        SCPI_NUM_DEF,
        SCPI_NUM_UP,
        SCPI_NUM_DOWN,
        SCPI_NUM_NAN,
        SCPI_NUM_INF,
        SCPI_NUM_NINF,
        SCPI_NUM_AUTO
    };
    typedef enum _scpi_special_number_t scpi_special_number_t;

    struct _scpi_choice_def_t {
        const char * name;
        int32_t tag;
    };
#define SCPI_CHOICE_LIST_END   {NULL, -1}
    typedef struct _scpi_choice_def_t scpi_choice_def_t;

    struct _scpi_param_list_t {
        const scpi_command_t * cmd;
        lex_state_t lex_state;
        scpi_const_buffer_t cmd_raw;
    };
    typedef struct _scpi_param_list_t scpi_param_list_t;

    struct _scpi_number_parameter_t {
        scpi_bool_t special;

        union {
            double value;
            int32_t tag;
        } content;
        scpi_unit_t unit;
        int8_t base;
    };
    typedef struct _scpi_number_parameter_t scpi_number_t;

    struct _scpi_data_parameter_t {
        const char * ptr;
        int32_t len;
    };
    typedef struct _scpi_data_parameter_t scpi_data_parameter_t;

    typedef scpi_token_t scpi_parameter_t;

    struct _scpi_command_t {
        const char * pattern;
        scpi_command_callback_t callback;
#if USE_COMMAND_TAGS
        int32_t tag;
#endif /* USE_COMMAND_TAGS */
    };

    struct _scpi_interface_t {
        scpi_error_callback_t error;
        scpi_write_t write;
        scpi_write_control_t control;
        scpi_command_callback_t flush;
        scpi_command_callback_t reset;
    };

    struct _scpi_t {
        const scpi_command_t * cmdlist;
        scpi_buffer_t buffer;
        scpi_param_list_t param_list;
        scpi_interface_t * interface;
        int_fast16_t output_count;
        int_fast16_t input_count;
        scpi_bool_t first_output;
        scpi_bool_t cmd_error;
        scpi_fifo_t error_queue;
#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION && !USE_MEMORY_ALLOCATION_FREE
        scpi_error_info_heap_t error_info_heap;
#endif
        scpi_reg_val_t registers[SCPI_REG_COUNT];
        const scpi_unit_def_t * units;
        void * user_context;
        scpi_parser_state_t parser_state;
        const char * idn[4];
        size_t arbitrary_remaining;
    };

    enum _scpi_array_format_t {
        SCPI_FORMAT_ASCII = 0,
        SCPI_FORMAT_NORMAL = 1,
        SCPI_FORMAT_SWAPPED = 2,
        SCPI_FORMAT_BIGENDIAN = SCPI_FORMAT_NORMAL,
        SCPI_FORMAT_LITTLEENDIAN = SCPI_FORMAT_SWAPPED,
    };
    typedef enum _scpi_array_format_t scpi_array_format_t;

#ifdef  __cplusplus
}
#endif

#endif  /* SCPI_TYPES_H */

