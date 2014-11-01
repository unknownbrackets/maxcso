ZSO format
===========

Please note that this format is not final, and is experimental.

This format has been proposed by [codestation][] in a patch to [procfw][].


Overview
===========

The general format is the same as the CSO v1 format.  It consists of a file header, index
section, and data section.

Unlike the original CSO format, blocks are compressed using [lz4][] rather than [deflate][].
Additionally, the magic bytes differ (ZISO), and the preferred extension is "zso".


Format
===========

The header is as follows (little endian):

    char[4]  magic;             // Always "ZISO".
	uint32_t header_size;       // Always 0x18.
	uint64_t uncompressed_size; // Total size of original ISO.
	uint32_t block_size;        // Size of each block, usually 2048.
	uint8_t  version;           // Always 1.
	uint8_t  index_shift;       // Indicates left shift of index values.
	uint8_t  unused[2];         // Always 0.

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

The final index entry indicates the end of the data segment and normally EOF.


[codestation]: https://github.com/codestation
[procfw]: https://code.google.com/p/procfw/
[lz4]: https://code.google.com/p/lz4/
[deflate]: https://www.ietf.org/rfc/rfc1951.txt