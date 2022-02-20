/**
  *********************************************************************************
  *
  * @file    main.h
  * @brief   Header file for DEMO
  *
  * @version V1.0
  * @date    26 Jun 2019
  * @author  AE Team
  * @note
  *          Change Logs:
  *          Date            Author          Notes
  *          26 Jun 2019     AE Team         The first version
  *
  * Copyright (C) Shanghai Eastsoft Microelectronics Co. Ltd. All rights reserved.
  *
  * SPDX-License-Identifier: Apache-2.0
  *
  * Licensed under the Apache License, Version 2.0 (the License); you may
  * not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an AS IS BASIS, WITHOUT
  * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  **********************************************************************************
  */

#ifndef   __MAIN_H__
#define   __MAIN_H__

#include "ald_conf.h"

typedef struct env_s {
	uint8_t conn;
	uint8_t update;
	uint32_t button;
	int32_t x_pos;
	int32_t y_pos;
} env_t;

extern env_t env;
#endif
