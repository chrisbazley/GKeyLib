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
#include <stdlib.h>

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
    RingBuffer_destroy(rb[i]);
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
      break;
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
  _Optional RingBuffer *rb = malloc(offsetof(RingBuffer, buffer) + (1u << HistoryLog2));
  if (rb)
  {
    for (size_t i = 0; i < NumberOfBuffers; i++)
    {
      RingBuffer_init(&*rb, HistoryLog2);
    }
  }
  free(rb);
}
void RingBuffer_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Make/destroy", test1 },
    { "Make fail recovery", test2 },
    { "Destroy null", test3 },
    { "Initialise", test4 },
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); count ++)
  {
    printf("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    Fortify_EnterScope();

    unit_tests[count].test_func();

    Fortify_LeaveScope();
  }
}
