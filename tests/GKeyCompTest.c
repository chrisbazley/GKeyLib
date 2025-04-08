/*
 * GKeyLib test: Gordon Key compression
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
#include "GKeyComp.h"

/* Local headers */
#include "Tests.h"

enum
{
  NumberOfCompressors = 5,
  HistoryLog2 = 9,
  FortifyAllocationLimit = 2048
};

static void test1(void)
{
  /* Make/destroy */
  _Optional GKeyComp *comp[NumberOfCompressors];

  for (size_t i = 0; i < ARRAY_SIZE(comp); i++)
  {
    comp[i] = gkeycomp_make(HistoryLog2);
    assert(comp[i] != NULL);
  }

  for (size_t i = 0; i < ARRAY_SIZE(comp); i++)
    gkeycomp_destroy(comp[i]);
}

static void test2(void)
{
  /* Make fail recovery */
  _Optional GKeyComp *comp = NULL;
  unsigned long limit;

  for (limit = 0; limit < FortifyAllocationLimit; ++limit)
  {
    Fortify_SetNumAllocationsLimit(limit);
    comp = gkeycomp_make(HistoryLog2);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (comp != NULL)
      break;
  }
  assert(limit != FortifyAllocationLimit);

  assert(comp != NULL);

  gkeycomp_destroy(comp);
}

static void test3(void)
{
  /* Destroy null */
  gkeycomp_destroy(NULL);
}
void GKeyComp_tests(void)
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
