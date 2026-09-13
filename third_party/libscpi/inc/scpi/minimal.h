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
 * @file   scpi_minimal.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 * 
 * @brief  SCPI minimal implementation
 * 
 * 
 */

#ifndef SCPI_MINIMAL_H
#define	SCPI_MINIMAL_H

#include "scpi/types.h"

#ifdef	__cplusplus
extern "C" {
#endif

    scpi_result_t SCPI_Stub(scpi_t * context);
    scpi_result_t SCPI_StubQ(scpi_t * context);

    scpi_result_t SCPI_SystemVersionQ(scpi_t * context);
    scpi_result_t SCPI_SystemErrorNextQ(scpi_t * context);
    scpi_result_t SCPI_SystemErrorCountQ(scpi_t * context);
    scpi_result_t SCPI_StatusQuestionableEventQ(scpi_t * context);
    scpi_result_t SCPI_StatusQuestionableConditionQ(scpi_t * context);
    scpi_result_t SCPI_StatusQuestionableEnableQ(scpi_t * context);
    scpi_result_t SCPI_StatusQuestionableEnable(scpi_t * context);
    scpi_result_t SCPI_StatusOperationConditionQ(scpi_t * context);
    scpi_result_t SCPI_StatusOperationEventQ(scpi_t * context);
    scpi_result_t SCPI_StatusOperationEnableQ(scpi_t * context);
    scpi_result_t SCPI_StatusOperationEnable(scpi_t * context);
    scpi_result_t SCPI_StatusPreset(scpi_t * context);


#ifdef	__cplusplus
}
#endif

#endif	/* SCPI_MINIMAL_H */

