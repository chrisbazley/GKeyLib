/* Copyright 2026 Christopher Bazley
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GKeyComp.h"
#include "GKeyDecomp.h"
#include "Tests.h"

enum { InputCapacity = 769, EncodedCapacity = (InputCapacity * 9 + 7) / 8 };

static void check_round_trip(const unsigned char *input, size_t size,
                             unsigned int history_log_2)
{
  /* Extra decoded space lets the size assertion detect a trailing byte.
     Heap buffers keep these larger cases within the Z80 data/stack limits. */
  _Optional unsigned char *const encoded = malloc(EncodedCapacity);
  assert(encoded != NULL);
  _Optional unsigned char *const decoded = malloc(InputCapacity + 1);
  assert(decoded != NULL);
  assert(size <= InputCapacity);
  _Optional GKeyComp *comp = gkeycomp_make(history_log_2);
  assert(comp != NULL);
  GKeyParameters p = {input, size, encoded, EncodedCapacity, NULL, &*encoded};
  GKeyStatus status = gkeycomp_compress(&*comp, &p);
  if (size == 0)
    assert(status == GKeyStatus_Finished);
  else
    assert(status == GKeyStatus_OK);
  assert(p.in_size == 0);
  assert(p.in_buffer == input + size);
  assert(gkeycomp_compress(&*comp, &p) == GKeyStatus_Finished);
  const size_t encoded_size = EncodedCapacity - p.out_size;
  assert(p.out_buffer == &*encoded + encoded_size);
  gkeycomp_destroy(comp);

  _Optional GKeyDecomp *decomp = gkeydecomp_make(history_log_2);
  assert(decomp != NULL);
  p.in_buffer = &*encoded;
  p.in_size = encoded_size;
  p.out_buffer = decoded;
  p.out_size = (InputCapacity + 1);
  assert(gkeydecomp_decompress(&*decomp, &p) == GKeyStatus_OK);
  assert(p.in_size == 0);
  assert(p.in_buffer == &*encoded + encoded_size);
  assert(p.out_size == (InputCapacity + 1) - size);
  assert(p.out_buffer == &*decoded + size);
  assert(memcmp(&*decoded, input, size) == 0);
  gkeydecomp_destroy(decomp);
  free(decoded);
  free(encoded);
}

void GKeyRoundTrip_tests(void)
{
  _Optional unsigned char *const allocated_input = calloc(InputCapacity, 1);
  assert(allocated_input != NULL);
  unsigned char *const input = &*allocated_input;
  static const unsigned int history_sizes[] = {0, 1, 9};

  puts("Empty stream");
  check_round_trip(input, 0, 9);

  for (size_t h = 0; h < ARRAY_SIZE(history_sizes); ++h)
  {
    printf("All byte values, history log2 %u\n", history_sizes[h]);
    for (size_t i = 0; i < InputCapacity; ++i)
      input[i] = (unsigned char)i;
    check_round_trip(input, InputCapacity, history_sizes[h]);

    printf("Repeated bytes, history log2 %u\n", history_sizes[h]);
    memset(input, 0x5a, InputCapacity);
    check_round_trip(input, InputCapacity, history_sizes[h]);
  }
  free(allocated_input);
}
