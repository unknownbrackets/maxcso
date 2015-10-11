CSO format
===========

The original CSO format was created by BOOSTER.

This document includes an experimental v2 format of CSO, proposed by Unknown W. Brackets.


Overview
===========

A CSO file consists of a file header, index section, and data section.

Typically, the file extension .cso is used.


Format (version 1)
===========

The header is as follows (little endian):

    char[4]  magic;             // Always "CISO".
	uint32_t header_size;       // Does not always contain a reliable value.
	uint64_t uncompressed_size; // Total size of original ISO.
	uint32_t block_size;        // Size of each block, usually 2048.
	uint8_t  version;           // May be 0 or 1.
	uint8_t  index_shift;       // Indicates left shift of index values.
	uint8_t  unused[2];         // May contain any values.

Following that are index entries, which are each a uint32_t (little endian).  The number of
index entries can be found by taking `ceil(uncompressed_size / block_size) + 1`.

The lower 31 bits of each index entry, when shifted left by `index_shift`, indicate the
position within the file of the block's compressed data.  The length of the block is the
difference between this entry's offset and the following index entry's offset value.

Note that this size may be larger than the compressed or uncompressed data, if `index_shift` is
greater than 0.  The space between blocks may be padded with any byte, but NUL is recommended.

Note also that this means index entries must be incrementing.  Reordering or deduplication of
blocks is not supported.

The high bit of the index entry indicates whether the block is uncompressed.

When compressed, blocks are compressed using the raw [deflate][] algorithm, with window size
being 15 (when using zlib, specify -15 for no zlib header.)

The final index entry indicates the end of the data segment and normally EOF.


Format (version 2)
===========

EXPERIMENTAL

The header is more strictly defined:

    char[4]  magic;             // Always "CISO".
	uint32_t header_size;       // Must always be 0x18.
	uint64_t uncompressed_size; // Total size of original ISO.
	uint32_t block_size;        // Size of each block.
	uint8_t  version;           // Must be 2.
	uint8_t  index_shift;       // Indicates left shift of index values.
	uint8_t  unused[2];         // Must be 0.

The index data follows the same format as version 1, but the interpretation of the size and high
bit is handled differently.

In version 2, when the length of a compressed block (that is, the difference between two index
entry offset values) is >= `block_size`, the block must not be compressed.

Note again that when `index_shift` is greater than 0, the size may include additional padding.
If the compressed size plus this padding would result in `block_size` or more bytes, the data
must not be compressed (or decompressed.)  This won't result in any observed file size
difference, because the padding would have been wasted bytes anyway.

When the size of the compressed block is less than `block_size`, the data is always compressed.
The high bit of the index entry indicates which compression method has been used.  When it is
set, the data is compressed with [lz4][], otherwise it is compressed with [deflate][].

The final index entry must not have the high bit set.


[lz4]: https://code.google.com/p/lz4/
[deflate]: https://www.ietf.org/rfc/rfc1951.txt