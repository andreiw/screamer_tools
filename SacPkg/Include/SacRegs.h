/** @file
  SAC device support.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

/*
 * SAC console in support requires a revised
 * FPGA image.
 */
#define SAC_AW_REVISION  0x4157

#define SAC_CFG_TEXT_OUT  0x200
#define SAC_CFG_TEXT_IN   0x200
