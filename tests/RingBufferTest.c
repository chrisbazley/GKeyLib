/*
 * GKeyLib test: Ring buffer
 * Copyright (C) 2016 Christopher Bazley
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

/* ISO library headers */
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* GKeyLib headers */
#include "Internal/RingBuffer.h"

/* Local headers */
#include "Tests.h"

enum
{
  NumberOfBuffers = 5,
  HistoryLog2 = 9,
  FortifyAllocationLimit = 2048
};

static void test1(void)
{
  /* Make/destroy */
  _Optional RingBuffer *rb[NumberOfBuffers];

  for (size_t i = 0; i < ARRAY_SIZE(rb); i++)
  {
    rb[i] = RingBuffer_make(HistoryLog2);
    assert(rb[i] != NULL);
  }

  for (size_t i = 0; i < ARRAY_SIZE(rb); i++)
  {
    RingBuffer_destroy(rb[i]);
  }
}

static void test2(void)
{
  /* Make fail recovery */
  _Optional RingBuffer *rb = NULL;
  unsigned long limit;

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    rb = RingBuffer_make(HistoryLog2);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (rb != NULL)
    {
      break;
    }
  }
  assert(limit != FortifyAllocationLimit);

  assert(rb != NULL);

  RingBuffer_destroy(rb);
}

static void test3(void)
{
  /* Destroy null */
  RingBuffer_destroy(NULL);
}

static void test4(void)
{
  /* Initialise */
  _Optional RingBuffer *rb =
    malloc(offsetof(RingBuffer, buffer) + (1u << HistoryLog2));
  assert(rb != NULL);

  for (size_t i = 0; i < NumberOfBuffers; i++)
  {
    RingBuffer_init(&*rb, HistoryLog2);
    assert(rb->size == (1u << HistoryLog2));
    assert(rb->write_pos == 0);
    assert(!rb->filled);

    /* Dirty the ring before the next reinitialisation. */
    static const unsigned char input[] = {1, 2, 3};
    RingBuffer_write(&*rb, input, sizeof(input));
  }

  free(rb);
}

/* Expected bytes are in logical order, starting at the write position. */
static void check_bytes(const RingBuffer *ring, const unsigned char *expected,
                        size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    assert(RingBuffer_read_char(ring, i) == expected[i]);
  }
}

static void test5(void)
{
  static const unsigned char
    input[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
    partial[] = {0, 0, 0, 0, 0, 1, 2, 3},
    wrapped[] = {10, 11, 12, 13, 14, 15, 16, 17};
  _Optional RingBuffer *ring = RingBuffer_make(3);
  assert(ring != NULL);

  RingBuffer_write(&*ring, input, 0);
  assert(ring->write_pos == 0);
  assert(!ring->filled);

  RingBuffer_write(&*ring, input, 4);
  assert(ring->write_pos == 4);
  assert(!ring->filled);
  check_bytes(&*ring, partial, ARRAY_SIZE(partial));

  /* A single write longer than the ring crosses its end twice. */
  RingBuffer_write(&*ring, input, sizeof(input));
  assert(ring->write_pos == 6);
  assert(ring->filled);
  check_bytes(&*ring, wrapped, ARRAY_SIZE(wrapped));

  RingBuffer_reset(&*ring);
  assert(ring->write_pos == 0);
  assert(!ring->filled);

  for (size_t i = 0; i < ring->size; ++i)
  {
    assert(RingBuffer_read_char(&*ring, i) == 0);
  }

  RingBuffer_destroy(ring);
}

static void test6(void)
{
  static const unsigned char
    input[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    expected[] = {6, 7, 8, 9, 6, 7, 8, 9};
  _Optional RingBuffer *ring = RingBuffer_make(3);
  assert(ring != NULL);

  RingBuffer_write(&*ring, input, sizeof(input));
  assert(RingBuffer_copy(&*ring, NULL, &*ring, 4, 0) == 0);
  assert(ring->write_pos == 2);

  /* The source crosses the physical end of the ring. */
  assert(RingBuffer_copy(&*ring, NULL, &*ring, 4, 4) == 4);
  assert(ring->write_pos == 6);
  check_bytes(&*ring, expected, ARRAY_SIZE(expected));

  RingBuffer_destroy(ring);
}

typedef struct
{
  size_t available, written, calls;
  unsigned char bytes[8];
} CopyOutput;

static size_t copy_output(void *arg, const void *src, size_t size)
{
  CopyOutput *const output = arg;
  ++output->calls;

  if (size > output->available)
  {
    size = output->available;
  }

  assert(output->written + size <= sizeof(output->bytes));
  memcpy(output->bytes + output->written, src, size);
  output->written += size;
  output->available -= size;

  return size;
}

static void test7(void)
{
  static const unsigned char
    input[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    unchanged[] = {2, 3, 4, 5, 6, 7, 8, 9},
    expected[] = {6, 7, 8, 9, 6, 7, 8, 9},
    copied[] = {6, 7, 8, 9};
  CopyOutput output = {
    .available = 0,
    .written = 0,
    .calls = 0,
    .bytes = {0},
  };
  _Optional RingBuffer *ring = RingBuffer_make(3);
  assert(ring != NULL);

  RingBuffer_write(&*ring, input, sizeof(input));
  assert(RingBuffer_copy(&*ring, copy_output, &output, 4, 4) == 0);
  assert(output.calls == 1);
  assert(output.written == 0);
  assert(ring->write_pos == 2);
  check_bytes(&*ring, unchanged, ARRAY_SIZE(unchanged));

  output.available = 1;
  assert(RingBuffer_copy(&*ring, copy_output, &output, 4, 4) == 1);
  assert(output.calls == 2);
  assert(output.written == 1);
  assert(output.bytes[0] == 6);
  assert(ring->write_pos == 3);

  /* Resume with the same relative offset, crossing the source boundary. */
  output.available = 3;
  assert(RingBuffer_copy(&*ring, copy_output, &output, 4, 3) == 3);
  assert(output.calls == 4);
  assert(output.written == sizeof(copied));
  assert(output.available == 0);
  assert(memcmp(output.bytes, copied, sizeof(copied)) == 0);
  assert(ring->write_pos == 6);
  check_bytes(&*ring, expected, ARRAY_SIZE(expected));

  RingBuffer_destroy(ring);
}

static void test8(void)
{
  static const unsigned char input[] = {0x11, 0x22, 0x80, 0xff,
                                       0x11, 0x22, 0x80, 0xff, 0x11, 0x22};
  _Optional RingBuffer *ring = RingBuffer_make(3);
  assert(ring != NULL);

  assert(RingBuffer_find_char(&*ring, 0, 8, 0) == 0);
  assert(RingBuffer_find_char(&*ring, 0, 8, 1) == SIZE_MAX);

  RingBuffer_write(&*ring, input, sizeof(input));
  assert(RingBuffer_find_char(&*ring, 0, 8, 0xff) == 1);
  assert(RingBuffer_find_char(&*ring, 2, 6, 0xff) == 5);
  assert(RingBuffer_find_char(&*ring, 2, 3, 0xff) == SIZE_MAX);
  assert(RingBuffer_find_char(&*ring, 6, 2, 0x11) == 6);
  assert(RingBuffer_find_char(&*ring, 0, 8, 0) == SIZE_MAX);
  assert(RingBuffer_find_char(&*ring, 0, 0, 0xff) == SIZE_MAX);

  RingBuffer_destroy(ring);
}

static void test9(void)
{
  static const unsigned char
    input[] = {0x11, 0x22, 0x80, 0xff, 0x11, 0x22, 0x80, 0xff, 0x11, 0x22},
    different[] = {0x55};
  _Optional RingBuffer *ring = RingBuffer_make(3);
  assert(ring != NULL);

  RingBuffer_write(&*ring, input, sizeof(input));
  assert(RingBuffer_compare(&*ring, 0, 4, 0) == 0);
  assert(RingBuffer_compare(&*ring, 0, 4, 4) == 0);
  assert(RingBuffer_compare(&*ring, 0, 1, 1) < 0);
  assert(RingBuffer_compare(&*ring, 1, 0, 1) > 0);

  /* Equal prefixes with a mismatch after one range wraps around. */
  RingBuffer_write(&*ring, different, sizeof(different));
  assert(RingBuffer_compare(&*ring, 0, 4, 3) == 0);
  assert(RingBuffer_compare(&*ring, 0, 4, 4) > 0);
  assert(RingBuffer_compare(&*ring, 4, 0, 4) < 0);

  RingBuffer_destroy(ring);
}

void RingBuffer_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  } unit_tests[] = {
    {"Make/destroy", test1},
    {"Make fail recovery", test2},
    {"Destroy null", test3},
    {"Initialise", test4},
    {"Write, read, wrap and reset", test5},
    {"Copy across ring boundary", test6},
    {"Callback truncation and resumption", test7},
    {"Bounded character search", test8},
    {"Compare wrapped ranges", test9},
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); count++)
  {
    printf("Test %zu/%zu : %s\n", 1 + count, ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    Fortify_EnterScope();

    unit_tests[count].test_func();

    Fortify_LeaveScope();
  }
}
