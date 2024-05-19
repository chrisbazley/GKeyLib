/*
 * GKeyLib: Gordon Key decompression
 * Copyright (C) 2011 Christopher Bazley
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* GKeyDecomp.h provides a low-level interface to a decompressor that can
   decode data in Gordon Key's compressed format.

Dependencies: ANSI C library.
History:
  CJB: 22-Nov-10: Created this header file.
*/

#ifndef GKeyDecomp_h
#define GKeyDecomp_h

/* Local headers */
#include "GKey.h"

typedef struct GKeyDecomp GKeyDecomp;
   /*
    * Opaque definition of retained state for a decompressor.
    */

GKeyDecomp *gkeydecomp_make(unsigned int history_log_2);
   /*
    * Creates a decompressor by allocating memory for, and initialising,
    * internal buffers and data structures. The history_log_2 parameter
    * is the no. of bytes to look behind, in base 2 logarithmic form, and
    * must be the same as that used to compress the data.
    * Returns: If successful, a pointer to retained state for the new
    *          decompressor, otherwise NULL (not enough free memory).
    */

void gkeydecomp_destroy(GKeyDecomp */*decomp*/);
   /*
    * Frees memory that was previously allocated for a decompressor.
    * Does nothing if called with a null pointer.
    */

void gkeydecomp_reset(GKeyDecomp */*decomp*/);
   /*
    * Resets a decompressor to a state suitable for decompressing a new
    * stream of data (as though newly created).
    */

GKeyStatus gkeydecomp_decompress(GKeyDecomp     */*decomp*/,
                                 GKeyParameters */*params*/);
   /*
    * Reads data from an input buffer and decompresses it, writing the
    * results to an output buffer. Can also be used to calculate the required
    * output buffer size, by providing no output buffer. Both buffers are
    * specified by the 'params' object. Treats the input as a continuation of
    * any data already consumed; it should be called iteratively until no
    * more input is available. If it returns TruncatedInput then the input
    * data was awkardly truncated (if the end of the bit stream doesn't
    * coincide with a byte boundary then any excess bits should be 0).
    * Returns: status of the decompressor (e.g. output buffer overflow).
    */

#endif
