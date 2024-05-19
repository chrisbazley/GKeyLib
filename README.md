# GKeyLib
(C) 2018 Christopher Bazley

Release 4 (19 May 2024)

Introduction
------------
  This is a C library for compressing or decompressing data using the same
algorithm used by old 4th Dimension and Fednet games such as 'Chocks Away',
'Stunt Racer 2000' and 'Star Fighter 3000'. It should have minimal
dependencies on other libraries and must not include any platform-specific
code.

  In 'Chocks Away' and 'Stunt Racer 2000' the contents of compressed files
were extracted by a module called 'DeComp' (©1993 The fourth dimension)
which provides a single command *CLoad - analogous to the standard command
*Load. In 'Star Fighter 3000' a similar module is supplied encrypted but it
is named 'FDComp' (© Gordon Key 1994) and its command is *LComp.

  Apart from an unbalanced stack bug having been fixed (when SWI OS_Find
returns an error) the two modules appear to be virtually identical. Actually
'FDComp' doesn't behave much better in the same situation because *LComp
returns with V set but R0 still pointing to the command tail rather than an
error block. It seems safe to assume that G.K. wrote both modules.

Compression format
------------------
  The first 4 bytes of a compressed file give the expected size of the data
when decompressed, as a 32 bit signed little-endian integer. Gordon Key's
file decompression module 'FDComp', which is presumably normative, rejects
input files where the top bit of the fourth byte is set (i.e. negative
values).

  Thereafter, the compressed data consists of tightly packed groups of 1, 8
or 9 bits without any padding between them or alignment with byte boundaries.
A decompressor must deal with two main types of chunk: The first (load a
byte) consists of 1+8 bits, and the second (copy data within output buffer)
consists of 1+9+8 or 1+9+9 bits.

The type of each chunk is determined by whether its first bit is set:

0.   Decode the next 8 bits of the input file (0-255) and write them as a
   byte at the current output position. The fact that this directive requires
   9 bits to represent 8 bits of information explains the inflation that can
   occur when attempting to compress random data.

1.   Decode the next 9 bits of the input file, which give an offset (0-511)
   within the data already decompressed, relative to a point 512 bytes behind
   the current output position. If this offset is greater than or equal to
   256 (i.e. within the last 256 bytes written) then decode the next 8 bits,
   which give the number of bytes (0-255) to be copied to the current output
   position. Otherwise, the next 9 bits give the number of bytes to be copied
   (0-511).

    If the read pointer is before the start of the output buffer then zeros
  should be written at the output position until it becomes valid again. This
  is a legitimate method of initialising areas of memory with zeros.

    A quirk of 'FDComp' is that least 1 byte is always written. That is
  probably a bug, although a well-written compressor should not insert
  directives to copy 0 bytes anyway. Note also that it isn't possible to
  replicate the whole of the preceding 512 bytes in one operation.

Fortified memory allocation
---------------------------
  I use Simon's P. Bullen's fortified memory allocation shell 'Fortify' to
find memory leaks in my applications, detect corruption of the heap
(e.g. caused by writing beyond the end of a heap block), and do stress
testing (by causing some memory allocations to fail). Fortify is available
separately from this web site:
http://web.archive.org/web/20020615230941/www.geocities.com/SiliconValley/Horizon/8596/fortify.html

  The debugging version of GKeyLib must be linked with 'Fortify', for
example by adding 'C:o.Fortify' to the list of object files specified to the
linker. Otherwise, you will get build-time errors like this:
```
ARM Linker: (Error) Undefined symbol(s).
ARM Linker:     Fortify_malloc, referred to from C:debug.GKeyLib(GKeyComp).
ARM Linker:     Fortify_free, referred to from C:debug.GKeyLib(GKeyComp).
```

Rebuilding the library
----------------------
  You should ensure that the standard C library and CBDebugLib (by the same
author as GKeyLib) are on your header include path (C$Path if using the
supplied make files on RISC OS), otherwise the compiler won't be able to find
the required header files. The dependency on CBDebugLib isn't very strong: it
can be eliminated by modifying the make file so that the macro USE_CBDEBUG is
no longer predefined.

  Three make files are supplied:

- 'Makefile' is intended for use with GNU Make and the GNU C Compiler on Linux.
- 'NMakefile' is intended for use with Acorn Make Utility (AMU) and the
   Norcroft C compiler supplied with the Acorn C/C++ Development Suite.
- 'GMakefile' is intended for use with GNU Make and the GNU C Compiler on RISC OS.

  These make files share some variable definitions (lists of objects to be
built) by including a common make file.

  The APCS variant specified for the Norcroft compiler is 32 bit for
compatibility with ARMv5 and fpe2 for compatibility with older versions of
the floating point emulator. Generation of unaligned data loads/stores is
disabled for compatibility with ARM v6.

  The suffix rules generate output files with different suffixes (or in
different subdirectories, if using the supplied make files on RISC OS),
depending on the compiler options used to compile the source code:

o: Assertions and debugging output are disabled. The code is optimised for
   execution speed.

oz: Assertions and debugging output are disabled. The code is suitable for
    inclusion in a relocatable module (multiple instantiation of static
    data and stack limit checking disabled). When the Norcroft compiler is
    used, the compiler optimises for smaller code size. (The equivalent GCC
    option seems to be broken.)

debug: Assertions and debugging output are enabled. The code includes
       symbolic debugging data (e.g. for use with DDT). The macro FORTIFY
       is pre-defined to enable Simon P. Bullen's fortified shell for memory
       allocations.

d: 'GMakefile' passes '-MMD' when invoking gcc so that dynamic dependencies
   are generated from the #include commands in each source file and output
   to a temporary file in the directory named 'd'. GNU Make cannot
   understand rules that contain RISC OS paths such as /C:Macros.h as
   prerequisites, so 'sed', a stream editor, is used to strip those rules
   when copying the temporary file to the final dependencies file.

  The above suffixes must be specified in various system variables which
control filename suffix translation on RISC OS, including at least
UnixEnv$ar$sfix, UnixEnv$gcc$sfix and UnixEnv$make$sfix.
Unfortunately GNU Make doesn't apply suffix rules to make object files in
subdirectories referenced by path even if the directory name is in
UnixEnv$make$sfix, which is why 'GMakefile' uses the built-in function
addsuffix instead of addprefix to construct lists of the objects to be
built (e.g. foo.o instead of o.foo).

  Before compiling the library for RISC OS, move the C source and header
files with .c and .h suffixes into subdirectories named 'c' and 'h' and
remove those suffixes from their names. You probably also need to create
'o', 'oz', 'd' and 'debug' subdirectories for compiler output.

Licence and disclaimer
----------------------
  This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

  This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
for more details.

  You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Credits
-------
  This library was derived from CBLibrary. Both libraries are free software,
available under the same license.

  Some credit goes to David O'Shea and Keith McKillop for working out the
Fednet compression algorithm. David wrote a DeComp module for use with the
Stunt Racer track designer, which I used as a reference when writing my own
decompression code.

History
-------
Release 1 (30 Sep 2018)
- Extracted the relevant components from CBLib to make a standalone library.

Release 2 (04 Nov 2018)
- Instead of all builds of the library depending on CBLib, now only the
  debug build has a dependency, and that is only on CBDebugLib.
- Some of the header files have been moved to an 'Internal' directory to
  discourage use by clients of the library. This includes the ring buffer
  interface. (It's hard to see how that could be useful for other purposes.)

Release 3 (28 Jul 2022)
- Fixed position of a linefeed in verbose debugging output from the
  compressor.
- Clarified documentation of typedef GKeyStatus and function
  gkeycomp_compress().

Release 4 (19 May 2024)
- Added dummy macro definitions to make the tests build without Fortify.
- Added new makefiles for use on Linux.
- Improved the README.md file for Linux users.

Contact details
---------------
Christopher Bazley

Email: mailto:cs99cjb@gmail.com

WWW:   http://starfighter.acornarcade.com/mysite/
