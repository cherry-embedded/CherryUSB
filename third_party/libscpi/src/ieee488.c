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
 * @file   scpi_ieee488.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 * 
 * @brief  Implementation of IEEE488.2 commands and state model
 * 
 * 
 */

#include "scpi/parser.h"
#include "scpi/ieee488.h"
#include "scpi/error.h"
#include "scpi/constants.h"

#include <stdio.h>

static const scpi_reg_info_t scpi_reg_details[SCPI_REG_COUNT] = {
    { SCPI_REG_CLASS_STB, SCPI_REG_GROUP_STB },
    { SCPI_REG_CLASS_SRE, SCPI_REG_GROUP_STB },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_ESR },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_ESR },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_COND, SCPI_REG_GROUP_OPER },
    { SCPI_REG_CLASS_EVEN, SCPI_REG_GROUP_QUES },
    { SCPI_REG_CLASS_ENAB, SCPI_REG_GROUP_QUES },
    { SCPI_REG_CLASS_COND, SCPI_REG_GROUP_QUES },

#if USE_CUSTOM_REGISTERS
#ifndef USER_REGISTER_DETAILS
#error "No user register details defined"
#else
    USER_REGISTER_DETAILS
#endif
#endif

};

static const scpi_reg_group_info_t scpi_reg_group_details[SCPI_REG_GROUP_COUNT] = {
    { 
        SCPI_REG_STB,
        SCPI_REG_SRE,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        0
    }, /* SCPI_REG_GROUP_STB */
    { 
        SCPI_REG_ESR,
        SCPI_REG_ESE,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_STB,
        STB_ESR
    }, /* SCPI_REG_GROUP_ESR */
    { 
        SCPI_REG_OPER,
        SCPI_REG_OPERE,
        SCPI_REG_OPERC,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_STB,
        STB_OPS
    }, /* SCPI_REG_GROUP_OPER */
    { 
        SCPI_REG_QUES,
        SCPI_REG_QUESE,
        SCPI_REG_QUESC,
        SCPI_REG_NONE,
        SCPI_REG_NONE,
        SCPI_REG_STB,
        STB_QES
    }, /* SCPI_REG_GROUP_QUES */

#if USE_CUSTOM_REGISTERS
#ifndef USER_REGISTER_GROUP_DETAILS
#error "No user register group details defined"
#else
    USER_REGISTER_GROUP_DETAILS
#endif
#endif

};

/**
 * Get register value
 * @param name - register name
 * @return register value
 */
scpi_reg_val_t SCPI_RegGet(scpi_t * context, scpi_reg_name_t name) {
    if ((name < SCPI_REG_COUNT) && context) {
        return context->registers[name];
    } else {
        return 0;
    }
}

/**
 * Wrapper function to control interface from context
 * @param context
 * @param ctrl number of controll message
 * @param value value of related register
 */
static size_t writeControl(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    if (context && context->interface && context->interface->control) {
        return context->interface->control(context, ctrl, val);
    } else {
        return 0;
    }
}

/**
 * Set register value
 * @param name - register name
 * @param val - new value
 */
void SCPI_RegSet(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t val) {
    if ((name >= SCPI_REG_COUNT) || (context == NULL)) {
        return;
    }

    scpi_reg_group_info_t register_group;

    do {
        scpi_reg_class_t register_type = scpi_reg_details[name].type;
        register_group = scpi_reg_group_details[scpi_reg_details[name].group];

        scpi_reg_val_t ptrans;

        /* store old register value */
        scpi_reg_val_t old_val = context->registers[name];

        if (old_val == val) {
            return;
        } else {
            context->registers[name] = val;
        }

        switch (register_type) {
            case SCPI_REG_CLASS_STB:
            case SCPI_REG_CLASS_SRE:
            {
                scpi_reg_val_t stb = context->registers[SCPI_REG_STB] & ~STB_SRQ;
                scpi_reg_val_t sre = context->registers[SCPI_REG_SRE] & ~STB_SRQ;

                if (stb & sre) {
                    ptrans = ((old_val ^ val) & val);
                    context->registers[SCPI_REG_STB] |= STB_SRQ;
                    if (ptrans & val) {
                        writeControl(context, SCPI_CTRL_SRQ, context->registers[SCPI_REG_STB]);
                    }
                } else {
                    context->registers[SCPI_REG_STB] &= ~STB_SRQ;
                }
                break;
            }
            case SCPI_REG_CLASS_EVEN:
            {
                scpi_reg_val_t enable;
                if(register_group.enable != SCPI_REG_NONE) {
                    enable = SCPI_RegGet(context, register_group.enable);
                } else {
                    enable = 0xFFFF;
                }

                scpi_bool_t summary = val & enable;

                name = register_group.parent_reg;
                val = SCPI_RegGet(context, register_group.parent_reg);
                if (summary) {
                    val |= register_group.parent_bit;
                } else {
                    val &= ~(register_group.parent_bit);
                }
                break;
            }
            case SCPI_REG_CLASS_COND:
            {
                name = register_group.event;

                if(register_group.ptfilt == SCPI_REG_NONE && register_group.ntfilt == SCPI_REG_NONE) {
                    val = ((old_val ^ val) & val) | SCPI_RegGet(context, register_group.event);
                } else {
                    scpi_reg_val_t ptfilt = 0, ntfilt = 0;
                    scpi_reg_val_t transitions;
                    scpi_reg_val_t ntrans;

                    if(register_group.ptfilt != SCPI_REG_NONE) {
                        ptfilt = SCPI_RegGet(context, register_group.ptfilt);
                    }

                    if(register_group.ntfilt != SCPI_REG_NONE) {
                        ntfilt = SCPI_RegGet(context, register_group.ntfilt);
                    }

                    transitions = old_val ^ val;
                    ptrans = transitions & val;
                    ntrans = transitions & ~ptrans;

                    val = ((ptrans & ptfilt) | (ntrans & ntfilt)) | SCPI_RegGet(context, register_group.event);
                }
                break;
            }
            case SCPI_REG_CLASS_ENAB:
            case SCPI_REG_CLASS_NTR:
            case SCPI_REG_CLASS_PTR:
                return;
        }
    } while(register_group.parent_reg != SCPI_REG_NONE);
}

/**
 * Set register bits
 * @param name - register name
 * @param bits bit mask
 */
void SCPI_RegSetBits(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t bits) {
    SCPI_RegSet(context, name, SCPI_RegGet(context, name) | bits);
}

/**
 * Clear register bits
 * @param name - register name
 * @param bits bit mask
 */
void SCPI_RegClearBits(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t bits) {
    SCPI_RegSet(context, name, SCPI_RegGet(context, name) & ~bits);
}

/**
 * *CLS - This command clears all status data structures in a device. 
 *        For a device which minimally complies with SCPI. (SCPI std 4.1.3.2)
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreCls(scpi_t * context) {
    SCPI_ErrorClear(context);
    int i;
    for (i = 0; i < SCPI_REG_GROUP_COUNT; ++i) {
        scpi_reg_name_t event_reg = scpi_reg_group_details[i].event;
        if (event_reg != SCPI_REG_STB) {
            SCPI_RegSet(context, event_reg, 0);
        }
    }
    return SCPI_RES_OK;
}

/**
 * *ESE
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreEse(scpi_t * context) {
    int32_t new_ESE;
    if (SCPI_ParamInt32(context, &new_ESE, TRUE)) {
        SCPI_RegSet(context, SCPI_REG_ESE, (scpi_reg_val_t) new_ESE);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

/**
 * *ESE?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreEseQ(scpi_t * context) {
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_ESE));
    return SCPI_RES_OK;
}

/**
 * *ESR?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreEsrQ(scpi_t * context) {
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_ESR));
    SCPI_RegSet(context, SCPI_REG_ESR, 0);
    return SCPI_RES_OK;
}

/**
 * *IDN?
 * 
 * field1: MANUFACTURE
 * field2: MODEL
 * field4: SUBSYSTEMS REVISIONS
 * 
 * example: MANUFACTURE,MODEL,0,01-02-01
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreIdnQ(scpi_t * context) {
    int i;
    for (i = 0; i < 4; i++) {
        if (context->idn[i]) {
            SCPI_ResultMnemonic(context, context->idn[i]);
        } else {
            SCPI_ResultMnemonic(context, "0");
        }
    }
    return SCPI_RES_OK;
}

/**
 * *OPC
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreOpc(scpi_t * context) {
    SCPI_RegSetBits(context, SCPI_REG_ESR, ESR_OPC);
    return SCPI_RES_OK;
}

/**
 * *OPC?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreOpcQ(scpi_t * context) {
    /* Operation is always completed */
    SCPI_ResultInt32(context, 1);
    return SCPI_RES_OK;
}

/**
 * *RST
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreRst(scpi_t * context) {
    if (context && context->interface && context->interface->reset) {
        return context->interface->reset(context);
    }
    return SCPI_RES_OK;
}

/**
 * *SRE
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreSre(scpi_t * context) {
    int32_t new_SRE;
    if (SCPI_ParamInt32(context, &new_SRE, TRUE)) {
        SCPI_RegSet(context, SCPI_REG_SRE, (scpi_reg_val_t) new_SRE);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

/**
 * *SRE?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreSreQ(scpi_t * context) {
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_SRE));
    return SCPI_RES_OK;
}

/**
 * *STB?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreStbQ(scpi_t * context) {
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_STB));
    return SCPI_RES_OK;
}

/**
 * *TST?
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreTstQ(scpi_t * context) {
    (void) context;
    SCPI_ResultInt32(context, 0);
    return SCPI_RES_OK;
}

/**
 * *WAI
 * @param context
 * @return 
 */
scpi_result_t SCPI_CoreWai(scpi_t * context) {
    (void) context;
    /* NOP */
    return SCPI_RES_OK;
}

