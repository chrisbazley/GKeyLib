/* Copyright 2026 Christopher Bazley
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <limits.h>
#include <string.h>

#include "Tests.h"
#include "TestData.h"

static void put_bits(TestData *data, size_t *bit_pos,
                     unsigned int width, unsigned int value)
{
  for (unsigned int bit = 0; bit < width; ++bit, ++*bit_pos)
  {
    assert(*bit_pos / CHAR_BIT < sizeof(data->encoded));
    if (value & (1u << bit))
    {
      data->encoded[*bit_pos / CHAR_BIT] |=
        (unsigned char)(1u << (*bit_pos % CHAR_BIT));
    }
  }
}

void TestData_build(TestData *data, const TestDirective *directives, size_t count)
{
  const size_t history_size = (size_t)1 << TestDataHistoryLog2;
  size_t bit_pos = 0;

  memset(data, 0, sizeof(*data));

  for (size_t i = 0; i < count; ++i)
  {
    const TestDirective *const d = &directives[i];

    if (d->kind == TestData_Literal)
    {
      assert(d->value <= UCHAR_MAX);
      assert(data->plain_size < sizeof(data->plain));

      data->plain[data->plain_size++] = (unsigned char)d->value;
      put_bits(data, &bit_pos, 1, 0);
      put_bits(data, &bit_pos, CHAR_BIT, d->value);
    }
    else
    {
      assert(d->kind == TestData_Copy);

      /* The format reserves one fewer length bit in the upper half. */
      const unsigned int width = d->value < history_size / 2 ?
        TestDataHistoryLog2 : TestDataHistoryLog2 - 1;
      assert(d->size > 0);
      assert(d->size < ((size_t)1 << width));
      assert(d->value + d->size <= history_size);

      put_bits(data, &bit_pos, 1, 1);
      put_bits(data, &bit_pos, TestDataHistoryLog2, d->value);
      put_bits(data, &bit_pos, width, (unsigned int)d->size);

      for (size_t j = 0; j < d->size; ++j)
      {
        const size_t source = data->plain_size + d->value;
        assert(data->plain_size < sizeof(data->plain));
        data->plain[data->plain_size++] = source < history_size ?
          0 : data->plain[source - history_size];
      }
    }
  }

  /* Unused high bits of the final byte stay zero. */
  data->encoded_size = (bit_pos + CHAR_BIT - 1) / CHAR_BIT;
}

void TestData_mixed(TestData *data)
{
  /* Initial zero history, literals, then a copy of ABC from ABCD. Avoid
     copying the most recent byte, matching the compressor's documented
     FOURTH_DIMENSION policy. Both copy-length widths and high bytes occur. */
  static const TestDirective directives[] = {
    {.kind = TestData_Copy, .value = 0, .size = 3},
    {.kind = TestData_Literal, .value = 'A', .size = 0},
    {.kind = TestData_Literal, .value = 'B', .size = 0},
    {.kind = TestData_Literal, .value = 'C', .size = 0},
    {.kind = TestData_Literal, .value = 'D', .size = 0},
    {.kind = TestData_Copy, .value = 508, .size = 3},
    {.kind = TestData_Literal, .value = 0xff, .size = 0},
    {.kind = TestData_Literal, .value = 0x80, .size = 0},
  };
  TestData_build(data, directives, ARRAY_SIZE(directives));
}
