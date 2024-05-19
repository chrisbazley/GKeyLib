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

/* History:
  CJB: 05-Jan-11: Created this source file.
  CJB: 07-Jan-11: Added the GKey_get_status_str function as a debugging aid.
                  Modified GKey_get_read_size_bits to cope with 1 byte history
                  and use DEBUG_VERBOSE.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Substituted format specifier %zu for %lu to avoid the need
                  to cast the matching parameter.
*/

/* ISO library header files */
#include <stddef.h>

/* Local headers */
#include "Internal/GKeyMisc.h"
#include "GKey.h"

unsigned int GKey_get_read_size_bits(unsigned int history_log_2, size_t read_offset)
{
  /* If the read offset is within the upper half of the ring buffer then the
     no. of bytes to copy can be encoded using fewer bits. '>' would give
     better coverage than '>=' but doesn't match Gordon Key's compression
     format (which allows bytes 255..511 or 257..511 to be copied but not
     256..511 !) */
  if (history_log_2 > 0 && read_offset >= 1u << (history_log_2 - 1))
    history_log_2--;

  DEBUG_VERBOSEF("GKey: read size from offset %zu requires %u bits\n",
                read_offset, history_log_2);

  return history_log_2;
}

const char *GKey_get_status_str(GKeyStatus status)
{
#ifdef DEBUG_OUTPUT
  static const char *strings[] =
  {
    "OK",
    "BadInput",
    "TruncatedInput",
    "BufferOverflow",
    "Aborted",
    "Finished"
  };
  assert(status < ARRAY_SIZE(strings));
  return strings[status - GKeyStatus_OK];
#else /* DEBUG_OUTPUT */
  NOT_USED(status);
  return "";
#endif /* DEBUG_OUTPUT */
}
