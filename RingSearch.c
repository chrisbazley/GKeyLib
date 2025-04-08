/*
 * GKeyLib: Ring buffer (search routines)
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

/* History:
  CJB: 19-Jan-11: Created this source file.
  CJB: 25-Jan-11: Added optimisation of RingBuffer_find_char based on
                  foreknowledge that the ring buffer is zero-initialized.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Substituted format specifier %zu for %lu to avoid the need
                  to cast the matching parameters.
*/

/* ISO library header files */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Local headers */
#include "Internal/RingBuffer.h"
#include "Internal/GKeyMisc.h"

int RingBuffer_read_char(const RingBuffer *ring, size_t offset)
{
  int c;

  assert(ring != NULL);
  assert(offset < ring->size);

  offset = (ring->write_pos + offset) & (ring->size - 1);
  c = ring->buffer[offset];

  DEBUG_VERBOSEF("RingBuffer: read 0x%02x from position %zu\n",
                c, offset);
  return c;
}

size_t RingBuffer_find_char(const RingBuffer *ring,
                            size_t            offset,
                            size_t            n,
                            int               c)
{
  size_t to_search, abs_read, found;
  _Optional const unsigned char *match;
  const unsigned char *start;
  bool search;

  assert(ring != NULL);
  assert(offset+n <= ring->size);

  DEBUG_VERBOSEF("RingBuffer: Searching %zu bytes for 0x%02x from offset %zu \n"
                "(pos %zu) in ring buffer",
                n, c, offset,
                ring->write_pos + offset);

  /* Calculate absolute read position within the buffer */
  abs_read = (ring->write_pos + offset) & (ring->size - 1);

  start = ring->buffer + abs_read;
  if (ring->write_pos > abs_read)
  {
    /* Check characters between start position and write position */
    to_search = ring->write_pos - abs_read;
    search = true;
  }
  else
  {
    /* Otherwise check characters between start position and end of buffer */
    assert(ring->size > abs_read);
    to_search = ring->size - abs_read;

    /* If the write position hasn't wrapped around yet then we know all
       characters from there to the end of the buffer are nul */
    search = ring->filled;
  }

  if (search)
  {
    /* Ensure we don't exceed the read limit imposed by the caller */
    to_search = LOWEST(to_search, n);

    DEBUG_VERBOSEF("RingBuffer: searching %p..%p\n",
                  start, start + to_search - 1);

    match = memchr(start, c, to_search);
  }
  else
  {
    DEBUG_VERBOSEF("RingBuffer: %p..%p is known to be zero\n",
                  start, start + to_search - 1);
    assert(*start == '\0');
    if (c == '\0')
      match = start; /* match at start position */
    else
      match = NULL; /* no possible match */
  }

  /* If there are more characters to be searched and we haven't found the
     character yet then restart at the beginning of the buffer */
  if (match == NULL && n > to_search)
  {
    /* Update read offset and size to reflect the number of bytes examined */
    offset += to_search;
    n -= to_search;

    /* Check characters between start of buffer and write position */
    start = ring->buffer;
    to_search = ring->write_pos;

    /* Ensure we don't exceed the read limit imposed by the caller */
    to_search = LOWEST(to_search, n);

    DEBUG_VERBOSEF("RingBuffer: searching %p..%p\n",
                  start, start + to_search - 1);

    match = memchr(start, c, to_search);
  }

  if (match != NULL)
  {
    found = match - start + offset;
    assert(found < ring->size);
    assert(ring->buffer[(ring->write_pos + found) & (ring->size - 1)] == c);

    DEBUG_VERBOSEF("RingBuffer: found match at %p (offset %zu, pos %zu)\n",
                  match, found,
                  ring->write_pos + found);
  }
  else
  {
    found = SIZE_MAX;
  }

  return found;
}

int RingBuffer_compare(const RingBuffer *ring,
                       size_t            offset1,
                       size_t            offset2,
                       size_t            n)
{
  size_t len1, len2, abs_read1, abs_read2, to_compare, nleft;
  const unsigned char *start1, *start2;
  int diff = 0;

  assert(ring != NULL);
  assert(offset1+n <= ring->size);
  assert(offset2+n <= ring->size);

  DEBUG_VERBOSEF("RingBuffer: Comparing %zu bytes at offset %zu (pos %zu) \n"
                "with offset %zu (pos %zu) in ring buffer",
                n, offset1,
                ring->write_pos + offset1,
                offset2,
                ring->write_pos + offset2);

  /* Calculate absolute read positions within the buffer */
  abs_read1 = (ring->write_pos + offset1) & (ring->size - 1);
  abs_read2 = (ring->write_pos + offset2) & (ring->size - 1);

  start1 = ring->buffer + abs_read1;
  start2 = ring->buffer + abs_read2;

  if (n == 1)
  {
    /* Single character compare */
    diff = *start1 - *start2;
  }
  else
  {
    /* Multiple character compare */
    if (ring->write_pos > abs_read1)
    {
      /* Check characters between start position and write position */
      len1 = ring->write_pos - abs_read1;
    }
    else
    {
      /* Otherwise until reaching the end of buffer */
      assert(ring->size > abs_read1);
      len1 = ring->size - abs_read1;
    }
    if (ring->write_pos > abs_read2)
    {
      /* Check characters between start position and write position */
      len2 = ring->write_pos - abs_read2;
    }
    else
    {
      /* Otherwise until reaching the end of buffer */
      assert(ring->size > abs_read2);
      len2 = ring->size - abs_read2;
    }

    /* Split the comparison into contiguous address ranges. This may require
       several iterations, because we need to restart upon reaching the end of
       the buffer (for either sequence) or the limit specified by the caller */
    for (nleft = n; nleft != 0; nleft -= to_compare)
    {
      /* Ensure we don't exceed the read limit imposed by the caller */
      to_compare = LOWEST(LOWEST(len1, len2), nleft);

      /* Compare the two contiguous address ranges */
      DEBUG_VERBOSEF("RingBuffer: comparing %p..%p with %p..%p\n",
                    start1, start1 + to_compare - 1,
                    start2, start2 + to_compare - 1);

      diff = memcmp(start1, start2, to_compare);
      if (diff != 0)
        break; /* found a mismatch */

      /* Find next contiguous address range of sequence 1 */
      assert(len1 >= to_compare);
      len1 -= to_compare;
      if (len1 == 0)
      {
        /* Check characters between start of buffer and write position */
        start1 = ring->buffer;
        len1 = ring->write_pos;
      }
      else
      {
        /* Haven't reached end of current contiguous address range */
        start1 += to_compare;
      }

      /* Find next contiguous address range of sequence 2 */
      assert(len2 >= to_compare);
      len2 -= to_compare;
      if (len2 == 0)
      {
        /* Check characters between start of buffer and write position */
        start2 = ring->buffer;
        len2 = ring->write_pos;
      }
      else
      {
        /* Haven't reached end of current contiguous address range */
        start2 += to_compare;
      }
    }
  }
  DEBUG_VERBOSEF("RingBuffer: Result of comparison is %s\n",
                diff < 0 ? "less" : diff == 0 ? "equal" : "greater");
  return diff;
}
