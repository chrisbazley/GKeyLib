# Codec unit tests

`GKeyCompTest.c` and `GKeyDecompTest.c` test the codecs independently. Neither
suite calls the other codec. `GKeyRoundTripTest.c` is a separate suite that
compresses and decompresses empty input, all byte values and repeated bytes.
Its 769-byte inputs wrap the 512-byte history buffer; it also tests history
sizes of one and two bytes.

`TestData_build` reads a `static const TestDirective[]` and produces both the
plain bytes and the packed stream specified by those directives. For example:

```c
static const TestDirective directives[] = {
  /* Three bytes from initial zero history. */
  {.kind = TestData_Copy, .value = 0, .size = 3},
  /* A literal 'A'. */
  {.kind = TestData_Literal, .value = 0x41, .size = 0},
};
```

The fixture history size is 512 bytes. A copy offset is measured from 512 bytes
behind the current write position; thus offset 508 copies from four bytes back.
The helper expands copies from earlier plain output (or initial zero history)
and packs fields least-significant bit first. It does not use the library's
ring buffer, bit-width helper, compressor or decompressor. Unused high bits of
the final byte are zero. A hand-calculated byte vector checks the assembler.

For compressor tests, choose directives that match its encoding policy. In
particular, it prefers the longest available match, uses literals when a copy
would cost more bits, and does not copy the most recently compressed byte
(`FOURTH_DIMENSION`). A valid stream need not be the stream the compressor would
choose. Decompressor fixtures are not subject to that restriction.

The mixed fixture covers initial zero history, literal bytes (including 0xff
and 0x80), a copy of prior literals, and both copy-length field widths. Each
codec is checked with full buffers, one-byte input/output buffers, and a null
output buffer with a nonzero initial size counter. Tests also check input and
output parameter updates, flush/finished behavior for compression, reset and
reuse, empty streams, and malformed/truncated decompression input.

The helper bounds its arrays and generated fields with assertions. Malformed
streams are specified directly as bytes rather than passed to the helper.
These tests are shared by the host and SDCC simulator builds; `MakeCommon` also
includes the helper for the legacy makefiles.

The codec output helpers take a numeric chunk size to run the same buffer
refill/resumption logic with full buffers or individual bytes. Size-calculation
tests are separate short functions: they need no output buffers or refill loop.

`RingBufferTest.c` also checks writes and reads across wraparound, reset to zero
history, copying across the physical boundary, callback refusal/truncation and
resumption, bounded character searches, and comparisons of wrapped ranges.

Round-trip input and output buffers are static arrays. Each output array is
filled with a known marker before each case; the input is initially zero and
fully assigned before each nonempty case. Only the compressed prefix reported
as written is decoded, and only the expected decoded length is compared after
checking the reported output length. SDCC places static data at 0xc000, below
the simulator interface and heap; its runner verifies code/data separation.
