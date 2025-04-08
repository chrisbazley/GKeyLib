/*
 * GKeyLib: Gordon Key game data compression
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
                  for frequent messages. Sequences are now reported as closed
                  intervals. write_bits() with nbits == 0 now behaves as
                  expected (flush moved to nbits == UINT_MAX). Fixed a bug
                  where the compressor could enter state Flush wrongly if the
                  input buffer were empty upon entering state FindSequence.
  CJB: 08-Jan-11: Defined FOURTH_DIMENSION (17% faster, not significantly worse
                  compression). gkeycomp_compress() no longer returns status
                  TruncatedInput (flush is always required anyway).
  CJB: 24-Jan-11: Rewrote find_sequence to search for the first character of a
                  sequence (using RingBuffer_find_char) and compare subsequent
                  characters with the previous best match (using
                  RingBuffer_compare) as distinct optimised steps.
  CJB: 06-Dec-14: Titivate debugging output.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 09-Apr-16: Modified assertions to avoid GNU C compiler warnings about
                  unsigned comparisons with signed integers.
  CJB: 15-May-16: Fixed a null pointer dereference in gkeycomp_destroy.
  CJB: 21-Jan-18: Made debugging output even less verbose.
  CJB: 29-Nov-20: Fixed position of linefeed in verbose debugging output.
  CJB: 08-Apr-25: Dogfooding the _Optional qualifier.
*/

/* ISO library header files */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/* Local headers */
#include "GKey.h"
#include "GKeyComp.h"
#include "Internal/RingBuffer.h"
#include "Internal/GKeyMisc.h"

/* Undefine this macro to allow the most recently compressed byte to be copied
   unless the start offset is 0 [sequence size would need to be 1 << n, but
   only n bits are available] or 1 << (n-1) [sequence size would need to be
   1 << (n-1), but only n-1 bits are available]. */
#define FOURTH_DIMENSION

/* We must be able to insert up to MAX(CHAR_BIT, MaxHistoryLog2) + 1 bits
   into an accumulator that already holds CHAR_BIT - 1 bits, because we can
   only output whole chars. MaxHistoryLog2 = 9 requires an accumulator >= 17
   bits wide. */
enum
{
  ULongMinBit    = 32, /* Minimum no. of bits in type 'unsigned long'. */
  MaxHistoryLog2 = ULongMinBit - CHAR_BIT /* Maximum no. of bytes to look
                                             behind, as a base 2 logarithm. */
};

/* All possible states of a compressor. The initial state must be zero. */
typedef enum
{
  GKeyCompState_NextSequence = -1,
  GKeyCompState_Progress,
  GKeyCompState_FindSequence,
  GKeyCompState_PutOffset,
  GKeyCompState_PutSize,
  GKeyCompState_PutByte,
  GKeyCompState_PutBytes,
  GKeyCompState_Flush
}
GKeyCompState;

struct GKeyComp
{
  GKeyCompState state;  /* Next action to do */
  size_t in_total;      /* Total no. of bytes consumed so far */
  size_t out_total;     /* Total no. of bytes output so far */
  size_t out_pos;       /* Position in ring buffer of next byte to output */
  size_t max_read_size; /* Maximum sequence size at the current start pos. */
  size_t best_read_offset; /* Offset of longest sequence found so far */
  size_t best_read_size;   /* Size of longest sequence found so far */
  size_t read_offset;      /* Offset from write position at which to start
                              copying data */
  size_t read_size;        /* No. of bytes to be copied */
  unsigned long acc;   /* Accumulator for bits waiting to be written to the
                          output buffer */
  char acc_nbits;      /* No. of bits valid in the accumulator */
  char history_log_2;  /* Size of ring buffer as a base 2 logarithm */
  RingBuffer *history; /* Ring buffer containing recently compressed data */
};

typedef struct
{
  GKeyComp *comp;
  GKeyParameters *params;
}
RingWriterParams;

static bool write_bits(GKeyComp *comp, GKeyParameters *params, unsigned int nbits, unsigned long bits)
{
  bool success = true;
  _Optional char *out_buffer;
  size_t out_size, out_total;
  unsigned int acc_nbits;
  unsigned long acc;

  assert(comp != NULL);
  assert(params != NULL);
  if (nbits != UINT_MAX)
  {
    assert(nbits <= ULongMinBit);
    assert(bits < 1ul << nbits);
  }

  DEBUG_VERBOSEF("GKeyComp: Writing %u bits (0x%lx) to output buffer %p of size %zu\n",
                 nbits, bits, params->out_buffer, params->out_size);

  out_buffer = params->out_buffer;
  out_size = params->out_size;
  out_total = comp->out_total;

  acc = comp->acc;
  acc_nbits = comp->acc_nbits;
  DEBUG_VERBOSEF("GKeyComp: Accumulator is 0x%lx (%u bits)\n", acc, acc_nbits);

  /* Special case to allow remaining bits to be flushed out */
  if (nbits == UINT_MAX)
  {
    nbits = 0;
    if ((acc_nbits % CHAR_BIT) != 0)
      acc_nbits += CHAR_BIT - (acc_nbits % CHAR_BIT);
  }

  /* While there is at least one byte in the accumulator, write the least
     significant 8 bits to the output buffer. */
  while (acc_nbits >= CHAR_BIT)
  {
    unsigned long old_acc;

    if (out_buffer != NULL && out_size == 0)
    {
      /* Not enough space in the output buffer to write the required no.
         of bits */
      success = false;
      DEBUG_VERBOSEF("GKeyComp: output buffer is full\n");
      break; /* optimises better than loop condition */
    }

    /* Shift down upper bits of accumulator to take the place
      of those about to be output */
    old_acc = acc;
    acc >>= CHAR_BIT;
    acc_nbits -= CHAR_BIT;
    DEBUG_VERBOSEF("GKeyComp: Accumulator is 0x%lx (%u bits)\n", acc, acc_nbits);

    if (out_buffer != NULL)
    {
      /* Output lower bits of accumulator */
      *(out_buffer++) = (unsigned char)old_acc;
      --out_size;
    }
    else
    {
      /* No output buffer was provided so calculate required buffer size */
      ++out_size;
    }
    DEBUG_VERBOSEF("GKeyComp: Wrote byte %zu (0x%02lx) to bitstream\n",
                   out_total, old_acc & UCHAR_MAX);
    ++out_total;
  }

  if (success)
  {
    /* Insert value into the accumulator's higher bits */
    assert(acc_nbits <= ULongMinBit - nbits);
    acc |= bits << acc_nbits;
    acc_nbits += nbits;
    DEBUG_VERBOSEF("GKeyComp: Accumulator is 0x%lx (%u bits)\n", acc, acc_nbits);
  }

  params->out_buffer = out_buffer;
  params->out_size = out_size;

  comp->out_total = out_total;
  comp->acc = acc;
  comp->acc_nbits = acc_nbits;

  return success;
}

static size_t ring_writer(_Optional void *arg, const void *src, size_t n)
{
  assert(arg);
  RingWriterParams *rwp = (void *)arg;
  size_t nout;
  GKeyComp *comp;
  GKeyParameters *params;
  const unsigned char *literals = (const unsigned char *)src;

  assert(rwp != NULL);
  assert(src != NULL || n == 0);
  DEBUG_VERBOSEF("GKeyComp: copying %zu bytes from %p\n", n, src);

  comp = rwp->comp;
  params = rwp->params;

  /* Write as many literal byte values to the output buffer as will fit. */
  for (nout = 0; nout < n; ++nout)
  {
    if (!write_bits(comp,
                    params,
                    CHAR_BIT + 1,
                    (unsigned long)literals[nout] << 1))
      break;
  }

  /* Return the no. of bytes copied */
  return nout;
}

static bool find_sequence(GKeyComp *comp, GKeyParameters *params)
{
  bool success;
  size_t read_offset, read_size, max_read_size, best_read_size, consumed, old_read_offset;
  int old_byte, new_byte;

  DEBUG_VERBOSEF("GKeyComp: Searching for match in recent history\n");
  assert(comp != NULL);
  assert(params != NULL);

  read_offset = comp->read_offset;
  read_size = comp->read_size;
  max_read_size = comp->max_read_size;
  best_read_size = comp->best_read_size;

  /* Search the ring buffer for sequences matching the input data. */
  for (consumed = 0; ; ++read_offset, read_size = 0)
  {
    if (read_size == 0)
    {
      /* Calculate the no. of characters to search for the start of a (longer)
         matching sequence */
      max_read_size = (size_t)(1ul << comp->history_log_2) - read_offset;
#ifdef FOURTH_DIMENSION
      /* The Fourth Dimension's Comp module never writes directives to copy
         the most recently compressed byte. */
      if (max_read_size > 0)
        --max_read_size;
#endif /* FOURTH_DIMENSION */

      /* There's no point searching for the first character beyond the point
         where we stand no chance of improving on the best sequence so far. */
      if (best_read_size >= max_read_size)
      {
        DEBUG_VERBOSEF("GKeyComp: Can't find sequence longer than %zu to beat %zu\n",
                       max_read_size, best_read_size);
        break;
      }

      if (best_read_size == 0)
      {
        /* Get the next byte of data to be compressed */
        const GKeyParameters * const p = params; /* Compiler being silly */
        if (consumed >= p->in_size)
        {
          DEBUG_VERBOSEF("GKeyComp: Out of input data (consumed %zu of %zu)\n",
                         consumed, p->in_size);
          break; /* No more data in input buffer */
        }

        new_byte = ((unsigned char *)p->in_buffer)[consumed];
      }
      else
      {
        /* Get the first byte of the previous longest matching sequence */
        new_byte = RingBuffer_read_char(comp->history, comp->best_read_offset);
      }

      /* First, search for the first character without bothering to update
         the maximum sequence length after every mismatch */
      old_read_offset = read_offset;
      read_offset = RingBuffer_find_char(comp->history,
                                         read_offset,
                                         max_read_size - best_read_size,
                                         new_byte);
      assert(read_offset >= old_read_offset);
      if (read_offset == SIZE_MAX)
      {
        DEBUG_VERBOSEF("GKeyComp: First character of sequence not found\n");
        max_read_size = 0;
        break;
      }

      if (read_size++ >= best_read_size)
      {
        DEBUG_VERBOSEF("GKeyComp: Consuming input byte 0x%02x at %zu\n",
                       new_byte, comp->in_total + consumed);
        ++consumed; /* consume the byte of input */
      }

      /* Calculate the maximum sequence length for the new start position */
      max_read_size -= read_offset - old_read_offset;
      assert(max_read_size > best_read_size);
#ifndef FOURTH_DIMENSION
      {
        /* Allow the most recently compressed byte to be copied provided that
           sufficient bits are allocated for the sequence size */
        size_t bits_limit;
        unsigned int nbits = GKey_get_read_size_bits(comp->history_log_2,
                                                     read_offset);
        bits_limit = (size_t)(1ul << nbits) - 1;
        if (max_read_size > bits_limit)
        {
          /* If the current sequence can't grow longer than the longest
             previously found then we have finished. */
          max_read_size = bits_limit;
          if (max_read_size <= best_read_size)
          {
            DEBUG_VERBOSEF("GKeyComp: Can't extend sequence beyond %zu to beat %zu\n",
                           max_read_size, best_read_size);
            break;
          }
        }
      }
#endif /* FOURTH_DIMENSION */
      DEBUG_VERBOSEF("GKeyComp: Maximum sequence length for offset %zu is %zu\n",
                    read_offset, max_read_size);

      /* Try to match the rest of the previous longest matching sequence */
      if (read_size < best_read_size)
      {
        if (RingBuffer_compare(comp->history,
                               read_offset + read_size,
                               comp->best_read_offset + read_size,
                               best_read_size - read_size) != 0)
        {
          DEBUG_VERBOSEF("GKeyComp: Mismatch between previous best sequence at "
                        "%zu and new sequence at %zu\n",
                        comp->best_read_offset,
                        read_offset);
          continue; /* search for the next instance of the first character */
        }
        read_size = best_read_size;
      }
    }

    /* Try to extend the matching sequence beyond the previous longest */
    for (; read_size < max_read_size; ++read_size)
    {
      /* Get the next byte of data to be compressed */
      const GKeyParameters * const p = params; /* Compiler being silly */
      if (consumed >= p->in_size)
      {
        DEBUG_VERBOSEF("GKeyComp: Out of input data (consumed %zu of %zu)\n",
                       consumed, p->in_size);
        goto finished; /* No more data in input buffer */
      }

      new_byte = ((unsigned char *)p->in_buffer)[consumed];

      /* Get next byte of current sequence from history ring buffer */
      old_byte = RingBuffer_read_char(comp->history, read_offset + read_size);
      if (new_byte != old_byte)
      {
        DEBUG_VERBOSEF("GKeyComp: Mismatch at %zu (0x%02x != 0x%02x)\n",
                      read_offset + read_size,
                      new_byte, old_byte);
        break;
      }

      DEBUG_VERBOSEF("GKeyComp: Consuming input byte 0x%02x at %zu\n",
                     new_byte, comp->in_total + consumed);
      ++consumed; /* consume the byte of input */
    }

    /* Mismatch with previously-compressed data or sequence has reached size
       limit so search for a new matching sequence */
    if (read_size > best_read_size)
    {
      DEBUG_VERBOSEF("GKeyComp: Replacing best match %zu..%zu with %zu..%zu\n",
                     comp->best_read_offset,
                     comp->best_read_offset + best_read_size - 1,
                     read_offset,
                     read_offset + read_size - 1);

      comp->best_read_offset = read_offset;
      best_read_size = read_size;
    }
  }

finished:
  comp->in_total += consumed;

  {
    GKeyParameters * const p = params; /* Compiler being silly */
    p->in_buffer = (unsigned char *)p->in_buffer + consumed;
    assert(p->in_size >= consumed);
    p->in_size -= consumed;
  }

  if (best_read_size >= max_read_size)
  {
    /* We have found the longest sequence (assuming that the maximum sequence
       length for higher start positions would be shorter) */
    comp->read_size = best_read_size;
    comp->read_offset = comp->best_read_offset;
    success = true;
  }
  else
  {
    /* Stalled due to lack of input data (current search is incomplete) */
    comp->read_size = read_size;
    comp->read_offset = read_offset;
    success = false;
  }

  DEBUG_VERBOSEF("GKeyComp: Found sequence %zu..%zu (%s)\n",
                 comp->read_offset, comp->read_offset + comp->read_size - 1,
                 success ? "final" : "stalled");

  comp->max_read_size = max_read_size;
  comp->best_read_size = best_read_size;

  return success;
}

static const char *get_state_str(GKeyCompState state)
{
#ifdef DEBUG_OUTPUT
  static const char *strings[] =
  {
    "NextSequence",
    "Progress",
    "FindSequence",
    "PutOffset",
    "PutSize",
    "PutByte",
    "PutBytes",
    "Flush"
  };
  assert(state - GKeyCompState_NextSequence >= 0);
  assert((size_t)(state - GKeyCompState_NextSequence) < ARRAY_SIZE(strings));
  return strings[state - GKeyCompState_NextSequence];
#else /* DEBUG_OUTPUT */
  NOT_USED(state);
  return "";
#endif /* DEBUG_OUTPUT */
}

_Optional GKeyComp *gkeycomp_make(unsigned int history_log_2)
{
  assert(history_log_2 <= MaxHistoryLog2);
  _Optional GKeyComp *comp = malloc(sizeof(*comp));
  if (comp != NULL)
  {
    memset(&*comp, 0, offsetof(GKeyComp, history_log_2));
    comp->history_log_2 = history_log_2;
    _Optional RingBuffer *const history = RingBuffer_make(history_log_2);
    if (history == NULL)
    {
      FREE_SAFE(comp);
    }
    else
    {
      comp->history = &*history;
    }
  }

  return comp;
}

void gkeycomp_destroy(_Optional GKeyComp *comp)
{
  if (comp != NULL)
  {
    RingBuffer_destroy(comp->history);
    free(comp);
  }
}

void gkeycomp_reset(GKeyComp *comp)
{
  assert(comp != NULL);
  memset(comp, 0, offsetof(GKeyComp, history_log_2));
  RingBuffer_reset(comp->history);
}

GKeyStatus gkeycomp_compress(GKeyComp       *comp,
                             GKeyParameters *params)
{
  GKeyStatus status = GKeyStatus_OK;
  GKeyCompState state;
  bool flush, input = true;
  const unsigned char *in_buffer;
  RingWriterParams rwp;
  size_t copied;
  unsigned int nbits;

  assert(comp != NULL);
  assert(params != NULL);

  state = comp->state;

  /* Treat no input as a special case that force-completes the current
     sequence then flushes any bits lingering in the accumulator. */
  flush = (params->in_size == 0);
  DEBUGF("GKeyComp: Will %sflush\n", flush ? "" : "not ");

  do
  {
    switch (state)
    {
      case GKeyCompState_NextSequence:
        /* Reset the compressor's state to find the next matching sequence */
        DEBUG_VERBOSEF("GKeyComp: Zeroing sequence parameters\n");
        comp->best_read_size = 0;
        comp->best_read_offset = 0;
        comp->read_size = 0;
        comp->read_offset = 0;
        /* FALLTHROUGH */

      case GKeyCompState_Progress:
        DEBUG_VERBOSEF("GKeyComp: Reporting progress (%zu in, %zu out)\n",
                       comp->in_total, comp->out_total);

        /* Do a callback to report progress, if a function was supplied. */
        if (params->prog_cb)
        {
          if (params->prog_cb(params->cb_arg, comp->in_total, comp->out_total))
            state = GKeyCompState_FindSequence;
          else
            status = GKeyStatus_Aborted;
        }
        else
        {
          state = GKeyCompState_FindSequence;
        }

        if (state != GKeyCompState_FindSequence)
          break;

        /* FALLTHROUGH */

      case GKeyCompState_FindSequence:
        /* Read bytes from the input buffer, updating the read offset and size
           to indicate a matching sequence in the ring buffer. */
        if (flush || find_sequence(comp, params))
        {
          /* Found the longest matching sequence (which may be empty). */
          if (comp->read_size == 0)
          {
            /* No match was found in the ring buffer */
            if (params->in_size > 0)
            {
              /* Put the unmatched byte as a literal value */
              state = GKeyCompState_PutByte;
            }
            else if (flush)
            {
              /* Flush unwritten bits out of the accumulator */
              state = GKeyCompState_Flush;
            }
            else
            {
              /* Ran out of input and the maximum sequence length is 0 */
              assert(comp->history_log_2 == 0);
              input = false;
            }
          }
          else
          {
            /* Put one or more literal values to the output if they would be
               smaller than the equivalent copy command. */
            nbits = GKey_get_read_size_bits(comp->history_log_2,
                                            comp->read_offset);
            if (comp->read_size * (CHAR_BIT + 1) <
                  comp->history_log_2 + nbits + 1)
              state = GKeyCompState_PutBytes;
            else
              state = GKeyCompState_PutOffset;
          }
        }
        else
        {
          /* Need to examine the next batch of input to extend the current
             match. */
          input = false;
        }

        if (state != GKeyCompState_PutOffset)
          break;
        /* FALLTHROUGH */

      case GKeyCompState_PutOffset:
        DEBUG_VERBOSEF("GKeyComp: Putting copy offset\n");
        if (write_bits(comp,
                       params,
                       comp->history_log_2 + 1,
                       ((unsigned long)comp->read_offset << 1) | 1))
          state = GKeyCompState_PutSize;
        else
          status = GKeyStatus_BufferOverflow;

        if (state != GKeyCompState_PutSize)
          break;
        /* FALLTHROUGH */

      case GKeyCompState_PutSize:
        DEBUG_VERBOSEF("GKeyComp: Putting copy size\n");
        /* If the read offset is within the upper half of the ring buffer then
           the no. of bytes to copy can be encoded using fewer bits. */
        nbits = GKey_get_read_size_bits(comp->history_log_2,
                                        comp->read_offset);
        if (write_bits(comp,
                       params,
                       nbits,
                       comp->read_size))
        {
          /* Copy matching sequence to the write position in the ring
             buffer. */
          copied = RingBuffer_copy(comp->history,
                                   (RingBufferWriteFn *)NULL,
                                   NULL,
                                   comp->read_offset,
                                   comp->read_size);
          assert(copied <= comp->read_size);
          state = GKeyCompState_NextSequence;
        }
        else
        {
          status = GKeyStatus_BufferOverflow;
        }
        break;

      case GKeyCompState_PutByte:
        DEBUG_VERBOSEF("GKeyComp: Putting unmatched byte\n");

        /* Encode unmatched byte as a literal value */
        in_buffer = params->in_buffer;
        if (write_bits(comp,
                       params,
                       CHAR_BIT + 1,
                       (unsigned long)*in_buffer << 1))
        {
          /* Write unmatched byte into the ring buffer */
          RingBuffer_write(comp->history, params->in_buffer, 1);

          /* Consume the unmatched byte */
          DEBUG_VERBOSEF("GKeyComp: Consuming input byte 0x%02x at %zu\n",
                         *in_buffer, comp->in_total);
          params->in_buffer = in_buffer + 1;
          --params->in_size;
          ++comp->in_total;

          state = GKeyCompState_NextSequence;
        }
        else
        {
          status = GKeyStatus_BufferOverflow;
        }
        break;

      case GKeyCompState_PutBytes:
        DEBUG_VERBOSEF("GKeyComp: Putting sequence as literal values\n");
        rwp.comp = comp;
        rwp.params = params;
        copied = RingBuffer_copy(comp->history,
                                 ring_writer,
                                 &rwp,
                                 comp->read_offset,
                                 comp->read_size);
        assert(copied <= comp->read_size);
        if (copied >= comp->read_size)
        {
          state = GKeyCompState_NextSequence;
        }
        else
        {
          /* Failed to copy all the data so update the read size to reflect
             the changed write position. Read offset is relative to the write
             position, so no need to update that. */
          comp->read_size -= copied;
          status = GKeyStatus_BufferOverflow;
        }
        break;

      case GKeyCompState_Flush:
        DEBUG_VERBOSEF("GKeyComp: Flushing any bits left in the accumulator\n");
        if (write_bits(comp, params, UINT_MAX, 0))
          status = GKeyStatus_Finished;
        else
          status = GKeyStatus_BufferOverflow;
        /* We never leave this state because writing data after a flush would
           produce corrupt output */
        break;

      default:
        assert("Bad state" == NULL);
        break;
    }
  }
  while (status == GKeyStatus_OK && input);

  comp->state = state;

  DEBUGF("GKeyComp: Returning status %s in state %s\n",
         GKey_get_status_str(status),
         get_state_str(state));

  return status;
}
