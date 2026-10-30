/* Copyright 2026 Christopher Bazley
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "GKeyComp.h"
#include "GKeyDecomp.h"
#include "Tests.h"

enum { InputCapacity = 769, EncodedCapacity = (InputCapacity * 9 + 7) / 8 };

static void check_round_trip(const unsigned char *input, size_t size,
                             unsigned int history_log_2)
{
  /* Extra decoded space lets the size assertion detect a trailing byte. */
  static unsigned char encoded[EncodedCapacity], decoded[InputCapacity + 1];

  assert(size <= InputCapacity);

  /* Every byte has a known value, including output a broken codec might
     report as written without actually writing it. Refill for every case. */
  memset(encoded, 0x5a, sizeof(encoded));
  memset(decoded, 0xa5, sizeof(decoded));

  _Optional GKeyComp *comp = gkeycomp_make(history_log_2);
  assert(comp != NULL);

  GKeyParameters p = {
    .in_buffer = input,
    .in_size = size,
    .out_buffer = encoded,
    .out_size = EncodedCapacity,
    .prog_cb = NULL,
    .cb_arg = encoded,
  };

  GKeyStatus status = gkeycomp_compress(&*comp, &p);
  if (size == 0)
  {
    assert(status == GKeyStatus_Finished);
  }
  else
  {
    assert(status == GKeyStatus_OK);
  }

  assert(p.in_size == 0);
  assert(p.in_buffer == input + size);

  assert(gkeycomp_compress(&*comp, &p) == GKeyStatus_Finished);

  const size_t encoded_size = EncodedCapacity - p.out_size;
  assert(p.out_buffer == encoded + encoded_size);

  gkeycomp_destroy(comp);

  _Optional GKeyDecomp *decomp = gkeydecomp_make(history_log_2);
  assert(decomp != NULL);

  p.in_buffer = encoded;
  p.in_size = encoded_size;
  p.out_buffer = decoded;
  p.out_size = (InputCapacity + 1);

  assert(gkeydecomp_decompress(&*decomp, &p) == GKeyStatus_OK);

  assert(p.in_size == 0);
  assert(p.in_buffer == encoded + encoded_size);
  assert(p.out_size == (InputCapacity + 1) - size);
  assert(p.out_buffer == decoded + size);
  assert(memcmp(decoded, input, size) == 0);

  gkeydecomp_destroy(decomp);
}

void GKeyRoundTrip_tests(void)
{
  static unsigned char input[InputCapacity];
  static const unsigned int history_sizes[] = {0, 1, 9};

  puts("Empty stream");
  check_round_trip(input, 0, 9);

  for (size_t h = 0; h < ARRAY_SIZE(history_sizes); ++h)
  {
    printf("All byte values, history log2 %u\n", history_sizes[h]);
    for (size_t i = 0; i < InputCapacity; ++i)
    {
      input[i] = (unsigned char)i;
    }

    check_round_trip(input, InputCapacity, history_sizes[h]);

    printf("Repeated bytes, history log2 %u\n", history_sizes[h]);
    memset(input, 0x5a, InputCapacity);

    check_round_trip(input, InputCapacity, history_sizes[h]);
  }
}
