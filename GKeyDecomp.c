/*
 * GKeyLib: Gordon Key game data decompression
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
  CJB: 22-Nov-10: Created this source file.
  CJB: 07-Jan-11: Additional debugging output, and switched to DEBUG_VERBOSE
                  for frequent messages.
  CJB: 28-Jan-11: Now checks that the sum of the read offset and size doesn't
                  exceed the ring buffer size, to detect corrupt input.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 21-Apr-16: Added the log size of the ring buffer to struct GKeyDecomp
                  instead of getting it from within the RingBuffer, which is
                  now allocated elsewhere because it isn't strictly legal to
                  embed a struct with a flexible array member in another.
                  Substituted format specifier %zu for %lu to avoid the need
                  to cast the matching parameters.
  CJB: 15-May-16: Fixed a null pointer dereference in gkeydecomp_destroy.
  CJB: 21-Jan-18: Made debugging output even less verbose.
  CJB: 08-Apr-25: Dogfooding the _Optional qualifier.
*/

/* ISO library header files */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

/* Local headers */
#include "GKey.h"
#include "GKeyDecomp.h"
#include "Internal/RingBuffer.h"
#include "Internal/GKeyMisc.h"

/* We must be able to read up to MAX(CHAR_BIT, MaxHistoryLog2) + 1 bits from an
   accumulator with at least CHAR_BIT - 1 bits free, because we can only input
   whole chars. MaxHistoryLog2 = 9 requires an accumulator >= 17 bits wide. */
enum
{
  ULongMinBit    = 32, /* Minimum no. of bits in type 'unsigned long'. */
  MaxHistoryLog2 = ULongMinBit - CHAR_BIT /* Maximum no. of bytes to look
                                             behind, as a base 2 logarithm. */
};

/* All possible states of a decompressor. The initial state must be zero. */
typedef enum
{
  GKeyDecompState_Progress,
  GKeyDecompState_GetType,
  GKeyDecompState_GetOffset,
  GKeyDecompState_GetSize,
  GKeyDecompState_CopyData,
  GKeyDecompState_GetByte,
  GKeyDecompState_PutByte
}
GKeyDecompState;

/* This data structure is designed so that zero-initialisation produces a
   valid initial state */
struct GKeyDecomp
{
  GKeyDecompState state; /* Next action to do */
  size_t in_total;       /* Total no. of bytes consumed so far */
  size_t out_total;      /* Total no. of bytes output so far */
  size_t read_offset;    /* Offset from write position at which to start
                            copying data */
  size_t read_size;      /* No. of bytes to be copied */
  unsigned long acc;     /* Accumulator for bits read from the input buffer */
  char acc_nbits;        /* No. of bits valid in the accumulator */
  char literal;          /* Byte value to be written at the output position */
  char history_log_2;    /* Size of ring buffer as a base 2 logarithm */
  RingBuffer *history;   /* Ring buffer containing recently decompressed data */
};

typedef struct
{
  GKeyDecomp     *decomp;
  GKeyParameters *params;
}
RingWriterParams;

static size_t ring_writer(_Optional void *arg, const void *src, size_t n)
{
  GKeyParameters *params;
  assert(arg);
  RingWriterParams *rwp = (void *)arg;

  assert(rwp != NULL);
  assert(src != NULL || n == 0);

  params = rwp->params;

  if (params->out_buffer == NULL)
  {
    /* No output buffer was provided so update the required size. */
    params->out_size += n;
  }
  else
  {
    /* Copy as much of the source data into the output buffer as will fit. */
    char *const out_buffer = &*params->out_buffer;
    DEBUG_VERBOSEF("GKeyDecomp: %zu bytes free in output buffer\n",
                   params->out_size);

    if (params->out_size < n)
      n = params->out_size;

    DEBUG_VERBOSEF("GKeyDecomp: copying %zu bytes from %p to %p\n",
                   n, src, out_buffer);

    memcpy(out_buffer, src, n);

    /* Update the output buffer pointer and length to reflect the amount
       of data written to it. */
    params->out_buffer = out_buffer + n;
    params->out_size -= n;
  }

  /* Return the no. of bytes copied and update a running total */
  rwp->decomp->out_total += n;
  return n;
}

static bool read_bits(GKeyDecomp *decomp, GKeyParameters *params, unsigned int nbits, unsigned long *out)
{
  bool success = true;
  const unsigned char *in_buffer;
  size_t in_size, in_total;
  unsigned int acc_nbits;
  unsigned long acc;

  assert(decomp != NULL);
  assert(params != NULL);
  assert(nbits <= ULongMinBit);
  assert(out != NULL);

  DEBUG_VERBOSEF("GKeyDecomp: Reading %u bits from input buffer %p of size %zu\n",
                 nbits, params->in_buffer, params->in_size);
  in_buffer = params->in_buffer;
  in_size = params->in_size;
  acc = decomp->acc;
  acc_nbits = decomp->acc_nbits;
  in_total = decomp->in_total;

  /* While we don't have enough bits in the accumulator and there is more input
     available... */
  while (acc_nbits < nbits)
  {
    unsigned long byte;

    if (in_size == 0)
    {
      /* Not enough input available to read the requested no. of bits */
      success = false;
      break; /* optimises better than loop condition */
    }

    /* Consume a byte of input */
    byte = *(in_buffer++);
    DEBUG_VERBOSEF("GKeyDecomp: Read byte %zu (0x%02lx) from input buffer\n",
                   in_total, byte);
    ++in_total;
    --in_size;

    /* Insert higher bits in the accumulator */
    DEBUG_VERBOSEF("GKeyDecomp: Accumulator is 0x%lx (%u bits)\n",
                  acc, acc_nbits);
    assert(acc_nbits <= ULongMinBit - CHAR_BIT);
    acc |= byte << acc_nbits;
    acc_nbits += CHAR_BIT;
    DEBUG_VERBOSEF("GKeyDecomp: Accumulator is 0x%lx (%u bits)\n",
                  acc, acc_nbits);
  }

  decomp->in_total = in_total;
  params->in_buffer = in_buffer;
  params->in_size = in_size;

  /* If there are now enough bits in the accumulator then extract the requested
     number of bits from its value... */
  if (acc_nbits >= nbits)
  {
    /* Extract lower bits from the accumulator */
    const unsigned long mask = (1ul << nbits) - 1;
    DEBUG_VERBOSEF("GKeyDecomp: accumulator mask is 0x%lx\n", mask);

    *out = acc & mask;
    DEBUG_VERBOSEF("GKeyDecomp: Got value 0x%lx from bitstream\n", *out);

    /* Shift down upper bits of accumulator to take their place */
    acc >>= nbits;
    acc_nbits -= nbits;
    DEBUG_VERBOSEF("GKeyDecomp: Accumulator is now 0x%lx (%u bits)\n",
                  acc, acc_nbits);
  }

  decomp->acc = acc;
  assert(acc_nbits <= CHAR_MAX);
  decomp->acc_nbits = acc_nbits;

  return success;
}

static const char *get_state_str(GKeyDecompState state)
{
#ifdef DEBUG_OUTPUT
  static const char *strings[] =
  {
    "Progress",
    "GetType",
    "GetOffset",
    "GetSize",
    "CopyData",
    "GetByte",
    "PutByte"
  };
  assert(state - GKeyDecompState_Progress < ARRAY_SIZE(strings));
  return strings[state - GKeyDecompState_Progress];
#else /* DEBUG_OUTPUT */
  NOT_USED(state);
  return "";
#endif /* DEBUG_OUTPUT */
}

_Optional GKeyDecomp *gkeydecomp_make(unsigned int history_log_2)
{
  assert(history_log_2 <= MaxHistoryLog2);
  _Optional GKeyDecomp *decomp = malloc(sizeof(*decomp));
  if (decomp != NULL)
  {
    memset(&*decomp, 0, offsetof(GKeyDecomp, history_log_2));
    decomp->history_log_2 = history_log_2;
    _Optional RingBuffer *const history = RingBuffer_make(history_log_2);
    if (history == NULL)
    {
      FREE_SAFE(decomp);
    }
    else
    {
      decomp->history = &*history;
    }
  }

  return decomp;
}

void gkeydecomp_destroy(_Optional GKeyDecomp *decomp)
{
  if (decomp != NULL)
  {
    RingBuffer_destroy(decomp->history);
    free(decomp);
  }
}

void gkeydecomp_reset(GKeyDecomp *decomp)
{
  assert(decomp != NULL);
  memset(decomp, 0, offsetof(GKeyDecomp, history_log_2));
  RingBuffer_reset(decomp->history);
}

GKeyStatus gkeydecomp_decompress(GKeyDecomp *decomp, GKeyParameters *params)
{
  GKeyStatus status = GKeyStatus_OK;
  GKeyDecompState state;
  _Optional GKeyProgressFn *prog_cb;
  unsigned long bits;
  unsigned int nbits;
  bool stop = false;
  size_t copied;
  RingWriterParams rwp;

  assert(decomp != NULL);
  assert(params != NULL);

  state = decomp->state;
  prog_cb = params->prog_cb;

  do
  {
    switch (state)
    {
      case GKeyDecompState_Progress:
        /* Do a callback to report progress, if a function was supplied. */
        DEBUG_VERBOSEF("GKeyDecomp: Reporting progress (%zu in, %zu out)\n",
                       decomp->in_total, decomp->out_total);
        if (prog_cb)
        {
          if (prog_cb(params->cb_arg, decomp->in_total, decomp->out_total))
            state = GKeyDecompState_GetType;
          else
            status = GKeyStatus_Aborted;
        }
        else
        {
          state = GKeyDecompState_GetType;
        }
        if (state != GKeyDecompState_GetType)
          break;
        /* FALLTHROUGH */

      case GKeyDecompState_GetType:
        /* The type of each command is determined by whether its first bit is
           set */
        DEBUG_VERBOSEF("GKeyDecomp: Getting type of command\n");
        if (read_bits(decomp, params, 1, &bits))
        {
          if (bits)
            state = GKeyDecompState_GetOffset;
          else
            state = GKeyDecompState_GetByte;
        }
        else
        {
          /* Valid end of stream state (happens to coincide with the end of the
             previous command) */
          assert(decomp->acc == 0);
          assert(decomp->acc_nbits == 0);
          stop = true;
        }
        if (state != GKeyDecompState_GetOffset)
          break;
        /* FALLTHROUGH */

      case GKeyDecompState_GetOffset:
        /* Get an offset within the data already decompressed, from
           which to copy data to the current output position. */
        DEBUG_VERBOSEF("GKeyDecomp: Getting copy offset\n");
        if (read_bits(decomp, params, decomp->history_log_2, &bits))
        {
          /* The read offset is actually an offset from 1u << history_log_2
             bytes behind the write position but we can ignore that because
             the buffer is circular. */
          decomp->read_offset = (size_t)bits;
          state = GKeyDecompState_GetSize;
        }
        else
        {
          /* Not a valid end-of-stream state */
          status = GKeyStatus_TruncatedInput;
        }

        if (state != GKeyDecompState_GetSize)
          break;
        /* FALLTHROUGH */

      case GKeyDecompState_GetSize:
        DEBUG_VERBOSEF("GKeyDecomp: Getting copy size\n");
        /* If the read offset is within the upper half of the ring buffer then
           the no. of bytes to copy is encoded using fewer bits. */
        nbits = GKey_get_read_size_bits(decomp->history_log_2,
                                        decomp->read_offset);
        if (read_bits(decomp, params, nbits, &bits))
        {
          if (bits == 0 || decomp->read_offset + bits > (1ul << decomp->history_log_2))
          {
            /* A quirk of the FDComp module is that it treats 0 bits as 1.
               We are less tolerant of bad input. */
            status = GKeyStatus_BadInput;
          }
          else
          {
            decomp->read_size = (size_t)bits;
            state = GKeyDecompState_CopyData;
          }
        }
        else
        {
          /* Not a valid end-of-stream state */
          status = GKeyStatus_TruncatedInput;
        }

        if (state != GKeyDecompState_CopyData)
          break;
        /* FALLTHROUGH */

      case GKeyDecompState_CopyData:
        /* Copy bytes from the recently decompressed data to the
           current output pointer. */
        rwp.params = params;
        rwp.decomp = decomp;
        copied = RingBuffer_copy(decomp->history,
                                 ring_writer,
                                 &rwp,
                                 decomp->read_offset,
                                 decomp->read_size);
        assert(copied <= decomp->read_size);
        if (copied >= decomp->read_size)
        {
          state = GKeyDecompState_Progress; /* next command */
        }
        else
        {
          /* Failed to copy all the data so update the read size to reflect
             the changed write position. Read offset is relative to the write
             position, so no need to update that. */
          decomp->read_size -= copied;
          status = GKeyStatus_BufferOverflow;
        }
        break;

      case GKeyDecompState_GetByte:
        /* Get the next 8 bits as a literal byte value. */
        DEBUG_VERBOSEF("GKeyDecomp: Getting literal value\n");
        if (read_bits(decomp, params, CHAR_BIT, &bits))
        {
          assert(bits <= UCHAR_MAX);
          decomp->literal = (char)bits;
          state = GKeyDecompState_PutByte;
        }
        else
        {
          /* This may be a valid end-of-stream state because excess bits after
             the final command should be 0 (the first of which is interpreted
             as GKeyDecompState_GetByte). */
          if (decomp->acc == 0)
            stop = true;
          else
            status = GKeyStatus_TruncatedInput;
        }

        if (state != GKeyDecompState_PutByte)
          break;
        /* FALLTHROUGH */

      case GKeyDecompState_PutByte:
        /* Put a literal byte value at the output position. */
        DEBUG_VERBOSEF("GKeyDecomp: Putting literal value\n");
        rwp.params = params;
        rwp.decomp = decomp;
        if (ring_writer(&rwp, &decomp->literal, 1) == 1)
        {
          RingBuffer_write(decomp->history, &decomp->literal, 1);
          state = GKeyDecompState_Progress; /* next command */
        }
        else
        {
          status = GKeyStatus_BufferOverflow;
        }
        break;

      default:
        assert("Bad state" == NULL);
        break;
    }
  }
  while (status == GKeyStatus_OK && !stop);

  decomp->state = state;

  DEBUGF("GKeyDecomp: Returning status %s in state %s\n",
         GKey_get_status_str(status),
         get_state_str(state));

  return status;
}
