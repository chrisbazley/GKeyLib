/* Copyright 2026 Christopher Bazley
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef TestData_h
#define TestData_h

#include <stddef.h>

enum
{
  TestDataHistoryLog2 = 9,
  TestDataCapacity = 64,
  TestDataEncodedCapacity = 80
};

typedef enum { TestData_Literal, TestData_Copy } TestDataKind;

typedef struct
{
  TestDataKind kind;
  unsigned int value; /* Literal byte or copy offset. */
  size_t size;        /* Copy length (unused for literals). */
} TestDirective;

typedef struct
{
  unsigned char plain[TestDataCapacity];
  unsigned char encoded[TestDataEncodedCapacity];
  size_t plain_size, encoded_size;
} TestData;

/* Independently assemble a stream and expand its directives. No production
   compressor, decompressor, ring buffer or bit-width helper is used. */
void TestData_build(TestData *data, const TestDirective *directives, size_t count);
void TestData_mixed(TestData *data);

#endif
