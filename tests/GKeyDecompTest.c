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
#include <string.h>

/* GKeyLib headers */
#include "GKeyDecomp.h"

/* Local headers */
#include "Tests.h"
#include "TestData.h"

enum
{
  NumberOfDecompressors = 5,
  HistoryLog2 = 9,
  FortifyAllocationLimit = 2048
};

static void test1(void)
{
  /* Make/destroy */
  _Optional GKeyDecomp *decomp[NumberOfDecompressors];

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
  _Optional GKeyDecomp *decomp = NULL;
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

static void check_decompress(GKeyDecomp *decomp, const TestData *data,
                             size_t chunk_size)
{
  unsigned char output[TestDataCapacity];
  GKeyParameters p = {data->encoded, 0, output, sizeof(output), NULL, output};
  size_t supplied = 0, written = 0, calls = 0;
  assert(chunk_size > 0);
  do
  {
    p.in_size = data->encoded_size - supplied;
    if (p.in_size > chunk_size)
      p.in_size = chunk_size;
    supplied += p.in_size;
    GKeyStatus status;
    do
    {
      ++calls;
      assert(calls < 4 * (TestDataCapacity + TestDataEncodedCapacity));
      assert(written < sizeof(output));
      p.out_buffer = output + written;
      p.out_size = sizeof(output) - written;
      if (p.out_size > chunk_size)
        p.out_size = chunk_size;
      const size_t available = p.out_size;
      status = gkeydecomp_decompress(decomp, &p);
      written += available - p.out_size;
      assert(p.out_buffer == output + written);
      if (status == GKeyStatus_TruncatedInput)
        assert(supplied < data->encoded_size);
      else
        assert(status == GKeyStatus_OK || status == GKeyStatus_BufferOverflow);
    } while (status == GKeyStatus_BufferOverflow);
    assert(p.in_size == 0);
    assert(p.in_buffer == data->encoded + supplied);
    if (supplied == data->encoded_size)
      assert(status == GKeyStatus_OK);
  } while (supplied < data->encoded_size);
  assert(written == data->plain_size);
  assert(memcmp(output, data->plain, written) == 0);
}

static void test4(void)
{
  TestData data;
  TestData_mixed(&data);
  _Optional GKeyDecomp *decomp = gkeydecomp_make(TestDataHistoryLog2);
  assert(decomp != NULL);
  check_decompress(&*decomp, &data, TestDataCapacity);
  gkeydecomp_reset(&*decomp);
  check_decompress(&*decomp, &data, 1);
  /* With the old write position, this copy would read nonzero data.
     Reset must restore the initial zero history, not just parser state. */
  static const TestDirective initial_history[] = {{TestData_Copy, 508, 3}};
  TestData_build(&data, initial_history, ARRAY_SIZE(initial_history));
  gkeydecomp_reset(&*decomp);
  check_decompress(&*decomp, &data, TestDataCapacity);
  gkeydecomp_destroy(decomp);
}

static void test5(void)
{
  /* Hand-packed malformed streams; no compressor is involved. */
  static const struct
  {
    size_t size;
    GKeyStatus status;
    unsigned char bytes[3];
  } cases[] = {
    {3, GKeyStatus_BadInput, {0x01, 0x00, 0x00}}, /* Zero copy length. */
    {3, GKeyStatus_BadInput, {0xff, 0xff, 0x03}}, /* offset+size > 512. */
    {1, GKeyStatus_TruncatedInput, {0x01, 0x00, 0x00}}, /* Partial offset. */
    {2, GKeyStatus_TruncatedInput, {0x01, 0x00, 0x00}}, /* Partial length. */
    {1, GKeyStatus_TruncatedInput, {0x02, 0x00, 0x00}}, /* Partial literal. */
  };
  _Optional GKeyDecomp *decomp = gkeydecomp_make(TestDataHistoryLog2);
  assert(decomp != NULL);
  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    unsigned char output = 0xa5;
    GKeyParameters p = {cases[i].bytes, cases[i].size,
                        &output, 1, NULL, &output};
    gkeydecomp_reset(&*decomp);
    assert(gkeydecomp_decompress(&*decomp, &p) == cases[i].status);
    assert(p.in_size == 0);
    assert(p.in_buffer == cases[i].bytes + cases[i].size);
    assert(p.out_size == 1);
    assert(p.out_buffer == &output);
    assert(output == 0xa5);
  }
  gkeydecomp_destroy(decomp);
}

static void test6(void)
{
  static const TestDirective unused[] = {{TestData_Literal, 0, 0}};
  TestData data;
  TestData_build(&data, unused, 0);
  _Optional GKeyDecomp *decomp = gkeydecomp_make(TestDataHistoryLog2);
  assert(decomp != NULL);
  check_decompress(&*decomp, &data, TestDataCapacity);
  gkeydecomp_destroy(decomp);
}

static void test7(void)
{
  TestData data;
  TestData_mixed(&data);
  _Optional GKeyDecomp *decomp = gkeydecomp_make(TestDataHistoryLog2);
  assert(decomp != NULL);
  GKeyParameters p = {data.encoded, data.encoded_size, NULL, 7, NULL, &data};
  assert(gkeydecomp_decompress(&*decomp, &p) == GKeyStatus_OK);
  assert(p.in_size == 0);
  assert(p.in_buffer == data.encoded + data.encoded_size);
  assert(p.out_buffer == NULL);
  assert(p.out_size == 7 + data.plain_size);
  gkeydecomp_destroy(decomp);
}

void GKeyDecomp_tests(void)
{
  static const struct
  {
    const char *test_name;
    void (*test_func)(void);
  } unit_tests[] = {
    {"Make/destroy", test1},
    {"Make fail recovery", test2},
    {"Destroy null", test3},
    {"Directive decoding, streaming and reset", test4},
    {"Malformed and truncated directives", test5},
    {"Empty stream", test6},
    {"Calculate decompressed size", test7},
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
