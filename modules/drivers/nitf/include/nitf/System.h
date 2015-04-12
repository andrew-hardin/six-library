/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2010, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __NITF_SYSTEM_H__
#define __NITF_SYSTEM_H__

#include "nitf/Defines.h"
#include "nitf/Types.h"
#include "nitf/Error.h"
#include "nitf/Memory.h"
#include "nitf/DLL.h"
#include "nitf/Sync.h"
#include "nitf/Directory.h"
#include "nitf/IOHandle.h"
/*  Now we are cross-platform (except for IO -- see IOHandle.h)  */

NITFPROT(nitf_Uint16) nitf_System_swap16(nitf_Uint16 ins);
NITFPROT(nitf_Uint32) nitf_System_swap32(nitf_Uint32 inl);
NITFPROT(nitf_Uint32) nitf_System_swap64c(nitf_Uint64 inl);
NITFPROT(nitf_Uint64) nitf_System_swap64(nitf_Uint64 inl);

#endif