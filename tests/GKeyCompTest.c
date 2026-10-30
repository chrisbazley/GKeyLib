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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* GKeyLib headers */
#include "GKeyComp.h"

/* Local headers */
#include "Tests.h"
#include "TestData.h"

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

/* Compare exact bytes, including zero padding, without a decompressor.
   A chunk size of one exercises resumption within directives and flush. */
static void check_compress(GKeyComp *comp, const TestData *data,
                           size_t chunk_size)
{
  unsigned char output[TestDataEncodedCapacity];
  GKeyParameters p = {data->plain, 0, output, sizeof(output), NULL, output};
  size_t supplied = 0, written = 0, calls = 0;
  GKeyStatus status;
  assert(chunk_size > 0);
  do
  {
    /* Refill even after overflow: empty input requests a flush. */
    if (p.in_size == 0 && supplied < data->plain_size)
    {
      p.in_size = data->plain_size - supplied;
      if (p.in_size > chunk_size)
        p.in_size = chunk_size;
      supplied += p.in_size;
    }
    const bool flushing = p.in_size == 0;
    ++calls;
    assert(calls < 4 * (TestDataCapacity + TestDataEncodedCapacity));
    assert(written < sizeof(output));
    p.out_buffer = output + written;
    p.out_size = sizeof(output) - written;
    if (p.out_size > chunk_size)
      p.out_size = chunk_size;
    const size_t available = p.out_size;
    status = gkeycomp_compress(comp, &p);
    written += available - p.out_size;
    assert(p.out_buffer == output + written);
    if (status != GKeyStatus_BufferOverflow)
      assert(status == (flushing ? GKeyStatus_Finished : GKeyStatus_OK));
    assert(p.in_size <= supplied);
    assert(p.in_buffer == data->plain + (supplied - p.in_size));
  } while (status != GKeyStatus_Finished);
  assert(p.in_size == 0);
  assert(written == data->encoded_size);
  assert(memcmp(output, data->encoded, written) == 0);

  /* Finished compressors must ignore subsequent input. */
  p.in_buffer = data->plain;
  p.in_size = data->plain_size;
  const size_t remaining = p.out_size;
  assert(gkeycomp_compress(comp, &p) == GKeyStatus_Finished);
  assert(p.in_buffer == data->plain);
  assert(p.in_size == data->plain_size);
  assert(p.out_size == remaining);
}

static void test4(void)
{
  /* Check the assembler against a hand-packed vector: copy(0,3), literal A.
     LSB-first bits give 1 | (3 << 10) | (0x82 << 19). */
  static const TestDirective directives[] = {
    {TestData_Copy, 0, 3}, {TestData_Literal, 0x41, 0},
  };
  static const unsigned char encoded[] = {0x01, 0x0c, 0x10, 0x04};
  static const unsigned char plain[] = {0, 0, 0, 0x41};
  TestData data;
  TestData_build(&data, directives, ARRAY_SIZE(directives));
  assert(data.encoded_size == sizeof(encoded));
  assert(memcmp(data.encoded, encoded, sizeof(encoded)) == 0);
  assert(data.plain_size == sizeof(plain));
  assert(memcmp(data.plain, plain, sizeof(plain)) == 0);
  _Optional GKeyComp *comp = gkeycomp_make(TestDataHistoryLog2);
  assert(comp != NULL);
  check_compress(&*comp, &data, TestDataEncodedCapacity);
  gkeycomp_destroy(comp);
}

static void test5(void)
{
  TestData data;
  TestData_mixed(&data);
  _Optional GKeyComp *comp = gkeycomp_make(TestDataHistoryLog2);
  assert(comp != NULL);
  check_compress(&*comp, &data, TestDataEncodedCapacity);
  gkeycomp_reset(&*comp);
  check_compress(&*comp, &data, 1);
  /* Reset must discard the old nonzero history and restore zero history. */
  gkeycomp_reset(&*comp);
  check_compress(&*comp, &data, TestDataEncodedCapacity);
  gkeycomp_destroy(comp);
}

static void test6(void)
{
  static const TestDirective unused[] = {{TestData_Literal, 0, 0}};
  TestData data;
  TestData_build(&data, unused, 0);
  _Optional GKeyComp *comp = gkeycomp_make(TestDataHistoryLog2);
  assert(comp != NULL);
  check_compress(&*comp, &data, TestDataEncodedCapacity);
  gkeycomp_destroy(comp);
}

static void test7(void)
{
  TestData data;
  TestData_mixed(&data);
  _Optional GKeyComp *comp = gkeycomp_make(TestDataHistoryLog2);
  assert(comp != NULL);
  GKeyParameters p = {data.plain, data.plain_size, NULL, 7, NULL, &data};
  assert(gkeycomp_compress(&*comp, &p) == GKeyStatus_OK);
  assert(p.in_size == 0);
  assert(p.in_buffer == data.plain + data.plain_size);
  assert(gkeycomp_compress(&*comp, &p) == GKeyStatus_Finished);
  assert(p.out_buffer == NULL);
  assert(p.out_size == 7 + data.encoded_size);
  gkeycomp_destroy(comp);
}

void GKeyComp_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  } unit_tests[] = {
    {"Make/destroy", test1},
    {"Make fail recovery", test2},
    {"Destroy null", test3},
    {"Hand-packed directive vector", test4},
    {"Exact compression, streaming and reset", test5},
    {"Empty stream", test6},
    {"Calculate compressed size", test7},
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
