/*
 * GKeyLib: Ring buffer
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

/* RingBuffer.h provides a ring buffer used by the Gordon Key decompressor
   and compressor.

Dependencies: ANSI C library.
History:
  CJB: 28-Dec-10: Created this header file.
  CJB: 19-Jan-11: Added functions to search for a character or compare strings
                  within a ring buffer. Documentation improvements to existing
                  functions. Renamed some arguments.
  CJB: 25-Jan-11: Added a new member of RingBuffer to track when the content
                  of the ring buffer beyond the write position may not longer
                  be zeros.
  CJB: 10-Apr-16: Changed the struct member used to store the log buffer size
                  to a narrow type (char instead of unsigned int).
                  Added one function to allocate a buffer of a specified size
                  and another to deallocate a buffer. These were needed
                  because it isn't strictly legal to embed a struct with a
                  flexible array member in another struct.
*/

#ifndef RingBuffer_h
#define RingBuffer_h

/* ISO library header files */
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
  size_t size;             /* Size of ring buffer in bytes */
  size_t write_pos;        /* Position in buffer at which to write data */
  bool filled;             /* Has the write position wrapped around yet? */
  char size_log_2;         /* Size of ring buffer in base 2 logarithmic form */
  unsigned char buffer[];  /* Ring buffer is a variable-length array */
}
RingBuffer;

typedef size_t RingBufferWriteFn(void       */*arg*/,
                                 const void */*s*/,
                                 size_t      /*n*/);
   /*
    * Type of function called back to report a contiguous address range of data
    * about to be copied within the ring buffer. If it returns a value less
    * than 'n' then only that many bytes will be copied within the ring buffer
    * and RingBuffer_copy will return false.
    * Returns: number of bytes to be copied.
    */

RingBuffer *RingBuffer_make(unsigned int /*size_log_2*/);
   /*
    * Allocates and initializes a ring buffer of a given size,
    * specified as a power of 2.
    */

void RingBuffer_destroy(RingBuffer */*ring*/);
   /*
    * Deallocates a specified ring buffer.
    */

void RingBuffer_init(RingBuffer */*ring*/, unsigned int /*size_log_2*/);
   /*
    * Initialises a ring buffer of a given size, specified as a power of 2.
    */

void RingBuffer_reset(RingBuffer */*ring*/);
   /*
    * Resets a specified ring buffer to its initial state.
    */

void RingBuffer_write(RingBuffer */*ring*/, const void */*s*/, size_t /*n*/);
   /*
    * Writes 'n' characters from the object pointed to by 's' into a
    * specified ring buffer at its current write position, which is advanced
    * by the same number of characters.
    */

size_t RingBuffer_copy(RingBuffer        */*ring*/,
                       RingBufferWriteFn */*write_cb*/,
                       void              */*cb_arg*/,
                       size_t             /*offset*/,
                       size_t             /*n*/);
   /*
    * Copies 'n' characters within the specified ring buffer to its current
    * write position, which is advanced by the same number of characters. The
    * source is 'offset' characters beyond the current write position. Source
    * and destination may overlap but behaviour is undefined if offset+n is
    * greater than the buffer size. Unless the 'write_cb' argument is NULL it
    * specifies a function to be called back with each contiguous address
    * range before it is copied (passing the value of 'cb_arg'). If the
    * callback function truncates the address range then fewer characters
    * will be copied and this function will return false. After rectifying
    * the problem, the client may call this function again to finish copying.
    * Returns: the number of characters copied.
    */

int RingBuffer_read_char(const RingBuffer */*ring*/, size_t /*offset*/);
   /*
    * Reads a character from 'offset' characters beyond the current write
    * position in a specified ring buffer. Behaviour is undefined if 'offset'
    * is greater than or equal to the buffer size.
    * Returns: the character read, as an unsigned char converted to an int
    */

size_t RingBuffer_find_char(const RingBuffer */*ring*/,
                            size_t            /*offset*/,
                            size_t            /*n*/,
                            int               /*c*/);
   /*
    * Locates the first occurence of 'c' (converted to an unsigned char) in
    * the 'n' characters (each interpreted as unsigned char) at 'offset'
    * beyond the current write position of a specified ring buffer. Behaviour
    * is undefined if offset+n is greater than the buffer size.
    * Returns: offset from the write position to the matching character, or
    *          SIZE_MAX if not found.
    */

int RingBuffer_compare(const RingBuffer */*ring*/,
                       size_t            /*offset1*/,
                       size_t            /*offset2*/,
                       size_t            /*n*/);
   /*
    * Compares the first 'n' characters at 'offset1' beyond the current write
    * position in a specified ring buffer with the first 'n' characters at
    * 'offset2' beyond the current write position. Behaviour is undefined if
    * offset1+n or offset2+n is greater than the buffer size.
    * Returns: an integer greater than, equal to, or less than zero, according
    *          to whether the string at 'offset_1' is greater than, equal
    *          to, or less than the string at 'offset_2'.
    */
#endif
