/*
 * GKeyLib: Gordon Key compression
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

/* GKeyComp.h provides a low-level interface to a compressor that can encode
   data in Gordon Key's compressed format.
Dependencies: ANSI C library.
History:
  CJB: 22-Nov-10: Created this header file.
  CJB: 08-Jan-11: gkeycomp_compress() no longer returns status
                  TruncatedInput (flush is always required anyway).
  CJB: 06-Dec-20: Clarified documentation of gkeycomp_compress().
*/

#ifndef GKeyComp_h
#define GKeyComp_h

/* Local headers */
#include "GKey.h"

typedef struct GKeyComp GKeyComp;
   /*
    * Opaque definition of retained state for a compressor.
    */

GKeyComp *gkeycomp_make(unsigned int /*history_log_2*/);
   /*
    * Creates a compressor by allocating memory for, and initialising,
    * internal buffers and data structures. The history_log_2 parameter
    * is the no. of bytes to look behind, in base 2 logarithmic form, and
    * must be the same as that used to decompress the data.
    * Returns: If successful, a pointer to retained state for the new
    *          decompressor, otherwise NULL (not enough free memory).
    */

void gkeycomp_destroy(GKeyComp */*comp*/);
   /*
    * Frees memory that was previously allocated for a compressor.
    * Does nothing if called with a null pointer.
    */

void gkeycomp_reset(GKeyComp */*comp*/);
   /*
    * Resets a compressor to a state suitable for compressing a new stream
    * of data (as though newly created).
    */

GKeyStatus gkeycomp_compress(GKeyComp       */*comp*/,
                             GKeyParameters */*params*/);
   /*
    * Reads data from an input buffer and compresses it, writing the results
    * to an output buffer. Can also be used to calculate the required output
    * buffer size, by providing no output buffer. Both buffers are specified
    * by the 'params' object. Treats the input as a continuation of any data
    * already consumed; it should be called iteratively until no more input
    * is available. The client must call with an input buffer size of 0 to
    * flush pending output until GKeyStatus_Finished is returned (at which
    * point further input will be ignored).
    * This function never returns TruncatedInput or BadInput.
    * Returns: status of the compressor (e.g. output buffer overflow).
    */

#endif
