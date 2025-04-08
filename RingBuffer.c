/*
 * GKeyLib: Ring buffer (initialisation, write and copy routines)
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
  CJB: 28-Dec-10: Created this source file.
  CJB: 07-Jan-11: Switched to DEBUG_VERBOSE for frequent messages.
  CJB: 19-Jan-11: Moved the definition of RingBuffer_read_char to new source
                  file c.RingSearch (not used by decompressor). Renamed some
                  variables and changed a comparison to an equality test.
  CJB: 25-Jan-11: RingBuffer_write now records when the content of the ring
                  buffer beyond the write position may not longer be zeros.
  CJB: 28-Jan-11: Minor change to guard against overflow when a ring size
                  between UINT_MAX and ULONG_MAX is requested.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 10-Apr-16: Added one function to allocate a buffer of a specified size
                  and another to deallocate a buffer. These were needed
                  because it isn't strictly legal to embed a struct with a
                  flexible array member in another struct.
  CJB: 21-Apr-16: Substituted format specifier %zu for %lu to avoid the need
                  to cast the matching parameters.
  CJB: 30-May-16: Can now simulate failure of malloc in RingBuffer_make.
  CJB: 21-Jan-18: Made debugging output even less verbose.
  CJB: 08-Apr-25: Dogfooding the _Optional qualifier.
*/

/* ISO library header files */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Local headers */
#include "Internal/RingBuffer.h"
#include "Internal/GKeyMisc.h"

_Optional RingBuffer *RingBuffer_make(unsigned int size_log_2)
{
  _Optional RingBuffer * const ring = malloc(offsetof(RingBuffer, buffer) +
                                             (size_t)(1ul << size_log_2));
  if (ring != NULL)
  {
    RingBuffer_init(&*ring, size_log_2);
  }

  return ring;
}

void RingBuffer_destroy(_Optional RingBuffer *ring)
{
  free(ring);
}

void RingBuffer_init(RingBuffer *ring, unsigned int size_log_2)
{
  assert(ring != NULL);
  ring->size_log_2 = size_log_2;
  ring->size = (size_t)(1ul << size_log_2);
  RingBuffer_reset(ring);
}

void RingBuffer_reset(RingBuffer *ring)
{
  ring->write_pos = 0;
  ring->filled = false;
  memset(&ring->buffer, 0, ring->size);
}

void RingBuffer_write(RingBuffer *ring, const void *s, size_t n)
{
  size_t to_copy, nleft, write_pos, ring_size;

  assert(ring != NULL);
  assert(s != NULL || n == 0);

  write_pos = ring->write_pos;
  ring_size = ring->size;

  /* While we have copied more bytes to the output buffer than we have copied
     into the ring buffer... */
  for (nleft = n; nleft != 0; nleft -= to_copy)
  {
    /* Copy as much of the source data into the ring buffer as will fit before
       the end. */
    to_copy = ring_size - write_pos;
    if (to_copy > nleft)
      to_copy = nleft;

    DEBUG_VERBOSEF("RingBuffer: copying %zu bytes from %p to offset %zu\n",
                   to_copy, s, write_pos);

    memmove(ring->buffer + write_pos, s, to_copy);
    s = (const char *)s + to_copy;
    /* The caller is responsible for ensuring that [in .. in+n-1] is a
       contiguous address range when copying data within the ring buffer */

    /* Wrap around from the end of the ring buffer to its start */
    write_pos += to_copy;
    if (write_pos >= ring_size)
    {
      write_pos = 0;
      ring->filled = true;
    }

    /* If there's any more data then it will be written at the start of the
       buffer */
  }

  DEBUG_VERBOSEF("RingBuffer: write offset is now %zu\n", write_pos);
  ring->write_pos = write_pos;
}

size_t RingBuffer_copy(RingBuffer *ring, _Optional RingBufferWriteFn *write_cb, _Optional void *cb_arg, size_t offset, size_t n)
{
  size_t copied, to_copy, total;

  assert(ring != NULL);
  assert(offset + n <= ring->size);
  /* If the source data straddles the write position then its tail may be
     overwritten before being read. Also, juxtaposition of oldest and
     newest data makes no sense. */

  DEBUG_VERBOSEF("RingBuffer: Copying up to %zu bytes from %zu to %zu in ring buffer\n",
                 n, ring->write_pos + offset, ring->write_pos);

  copied = to_copy = 0;

  for (total = 0; total < n && copied >= to_copy; total += copied)
  {
    /* Copy as much of the source data before the end of the ring buffer as
       will fit in the output buffer. */
    const size_t read_pos = (ring->write_pos + offset) & (ring->size - 1);
    const void * const s = ring->buffer + read_pos;

    to_copy = ring->size - read_pos;
    if (to_copy > n - total)
      to_copy = n - total;

    /* If a callback function was provided then offer it the address range
       first so it can truncate it if necessary. */
    if (write_cb)
    {
      copied = write_cb(cb_arg, s, to_copy);
      assert(copied <= to_copy);
    }
    else
    {
      copied = to_copy;
    }

    /* Now copy the same data within the ring buffer */
    RingBuffer_write(ring, s, copied);
  }

  DEBUG_VERBOSEF("RingBuffer: Total bytes copied was %zu\n", total);
  return total;
}
