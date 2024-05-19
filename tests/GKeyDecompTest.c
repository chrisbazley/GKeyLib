/*
 * GKeyLib test: Gordon Key decompression
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

/* GKeyLib headers */
#include "GKeyDecomp.h"

/* Local headers */
#include "Tests.h"

enum
{
  NumberOfDecompressors = 5,
  HistoryLog2 = 9,
  FortifyAllocationLimit = 2048
};

static void test1(void)
{
  /* Make/destroy */
  GKeyDecomp *decomp[NumberOfDecompressors];

  for (size_t i = 0; i < ARRAY_SIZE(decomp); i++)
  {
    decomp[i] = gkeydecomp_make(HistoryLog2);
    assert(decomp[i] != NULL);
  }

  for (size_t i = 0; i < ARRAY_SIZE(decomp); i++)
    gkeydecomp_destroy(decomp[i]);
}

static void test2(void)
{
  /* Make fail recovery */
  GKeyDecomp *decomp = NULL;
  unsigned long limit;

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    decomp = gkeydecomp_make(HistoryLog2);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (decomp != NULL)
      break;
  }
  assert(limit != FortifyAllocationLimit);

  assert(decomp != NULL);

  gkeydecomp_destroy(decomp);
}

static void test3(void)
{
  /* Destroy null */
  gkeydecomp_destroy(NULL);
}
void GKeyDecomp_tests(void)
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
