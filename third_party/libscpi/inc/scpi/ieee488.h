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
 * @file   scpi_ieee488.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 * 
 * @brief  Implementation of IEEE488.2 commands and state model
 * 
 * 
 */

#ifndef SCPI_IEEE488_H
#define	SCPI_IEEE488_H

#include "scpi/types.h"

#ifdef  __cplusplus
extern "C" {
#endif

    scpi_result_t SCPI_CoreCls(scpi_t * context);
    scpi_result_t SCPI_CoreEse(scpi_t * context);
    scpi_result_t SCPI_CoreEseQ(scpi_t * context);
    scpi_result_t SCPI_CoreEsrQ(scpi_t * context);
    scpi_result_t SCPI_CoreIdnQ(scpi_t * context);
    scpi_result_t SCPI_CoreOpc(scpi_t * context);
    scpi_result_t SCPI_CoreOpcQ(scpi_t * context);
    scpi_result_t SCPI_CoreRst(scpi_t * context);
    scpi_result_t SCPI_CoreSre(scpi_t * context);
    scpi_result_t SCPI_CoreSreQ(scpi_t * context);
    scpi_result_t SCPI_CoreStbQ(scpi_t * context);
    scpi_result_t SCPI_CoreTstQ(scpi_t * context);
    scpi_result_t SCPI_CoreWai(scpi_t * context);


#define STB_R01 0x01    /* Not used */
#define STB_PRO 0x02    /* Protection Event Flag */
#define STB_QMA 0x04    /* Error/Event queue message available */
#define STB_QES 0x08    /* Questionable status */
#define STB_MAV 0x10    /* Message Available */
#define STB_ESR 0x20    /* Standard Event Status Register */
#define STB_SRQ 0x40    /* Service Request */
#define STB_OPS 0x80    /* Operation Status Flag */


#define ESR_OPC 0x01    /* Operation complete */
#define ESR_REQ 0x02    /* Request Control */
#define ESR_QER 0x04    /* Query Error */
#define ESR_DER 0x08    /* Device Dependent Error */
#define ESR_EER 0x10    /* Execution Error (e.g. range error) */
#define ESR_CER 0x20    /* Command error (e.g. syntax error) */
#define ESR_URQ 0x40    /* User Request */
#define ESR_PON 0x80    /* Power On */


    scpi_reg_val_t SCPI_RegGet(scpi_t * context, scpi_reg_name_t name);
    void SCPI_RegSet(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t val);
    void SCPI_RegSetBits(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t bits);
    void SCPI_RegClearBits(scpi_t * context, scpi_reg_name_t name, scpi_reg_val_t bits);

    void SCPI_EventClear(scpi_t * context);

#ifdef  __cplusplus
}
#endif

#endif	/* SCPI_IEEE488_H */

