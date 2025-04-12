/*
 * GKeyLib: Gordon Key compression/decompression (common)
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

/* GKey.h declares types and functions that are common to the compressor and
   decompressor for Gordon Key's compressed format.
Dependencies: None.
History:
  CJB: 22-Nov-10: Created this header file.
  CJB: 07-Jan-11: Added the GKey_get_status_str function as a debugging aid.
  CJB: 06-Dec-20: Clarified documentation of GKeyStatus.
  CJB: 08-Apr-25: Dogfooding the _Optional qualifier.
  CJB: 13-Apr-25: Allow a null context argument to the progress callback.
*/

#ifndef GKey_h
#define GKey_h

/* ISO library headers */
#include <stddef.h>
#include <stdbool.h>

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

typedef enum
{
  GKeyStatus_OK,             /* Operation completed successfully. */
  GKeyStatus_BadInput,       /* Input includes invalid compressed data. */
  GKeyStatus_TruncatedInput, /* Compressed input data is awkwardly truncated
                                (output may be incomplete if no more input is
                                provided). */
  GKeyStatus_BufferOverflow, /* Output buffer was too small to write all of
                                the output produced so far. */
  GKeyStatus_Aborted,        /* Operation aborted by a callback. */
  GKeyStatus_Finished        /* No further input will be accepted. */
}
GKeyStatus;
   /*
    * GKeyStatus is an enumeration of possible status values which may be
    * returned by functions which compress or decompress data using Gordon
    * Key's algorithm.
    */

typedef bool GKeyProgressFn(_Optional void *arg, size_t in, size_t out);
   /*
    * Type of function called back periodically whilst processing data, to
    * allow the client to display an indication of progress. The values of
    * 'in' and 'out' will be the total number of bytes consumed and output
    * so far. If this function returns false then the operation will be
    * aborted; otherwise it will continue until an error occurs or all
    * input has been consumed.
    */

typedef struct
{
  const void *in_buffer; /* Pointer to input buffer. Updated to point to
                            any data not consumed. */
  size_t     in_size;    /* Size of the input buffer, in bytes. Updated to
                            reflect the no. of bytes not consumed. */
  _Optional void *out_buffer; /* Pointer to output buffer. If it is null then the
                                 required output buffer size will be calculated.
                                 Otherwise, it is updated to point to any remaining
                                 free space. */
  size_t     out_size;   /* Size of the output buffer, in bytes. If out_buffer
                            is a null pointer then out_size will be incremented
                            by the no. of bytes not written; otherwise,
                            decremented by the no. of bytes written. */
  _Optional GKeyProgressFn *prog_cb; /* A function to be called to indicate progress
                                        during the operation, or a null pointer. */
  _Optional void           *cb_arg;  /* Context argument to be passed to the progress
                                        callback function. */
}
GKeyParameters;
   /*
    * GKeyParameters is an object that holds input and output parameters
    * common to functions which compress or decompress data using Gordon
    * Key's algorithm. It is designed so that the output values for one
    * call can be used as input values for the next, although intervention
    * to provide more input data or a new output buffer may be required.
    */

unsigned int GKey_get_read_size_bits(unsigned int /*history_log_2*/,
                                     size_t       /*read_offset*/);
   /*
    * Gets the number of bits allocated by Gordon Key's compression format to
    * represent the size of a copy operation starting at 'read_offset' bytes
    * from a position 'history_log_2' bytes behind the write position.
    * G.K. allows bytes 255..511 or 257..511 to be copied but not 256..511
    * (with history_log_2 == 9). This is an authentic quirk.
    * Returns: no. of bits sufficient for read size.
    */

const char *GKey_get_status_str(GKeyStatus status);
   /*
    * Gets a string representation of the specified status value.
    * Returns: pointer to a string, which must not be modified.
    */

#endif
