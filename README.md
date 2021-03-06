# QOIPond - Lossless image compression inspired by QOI “Quite OK Image” format

MIT licensed library for C/C++

⚠️ This should be in a functional state, but it's still an alpha WIP.

See [QOI](https://github.com/phoboslab/qoi) for the original format

## Why?

- QOI is fast and simple, but inflexible
- QOIPond builds on QOI by defining the set of opcodes used by a particular bitstream in the bitstream header

Flexibility like this means:
- The bitstream can be tailored to the input
- A size-optimising program can try many combinations of opcode and pick the set that best represents a given input within the search space (qoipconv can do this with png source, qoipcrunch can do this with qoip source)
- Caters to different use cases by allowing encode/decode/compression metrics to be balanced by the user
- General case performance takes a hit relative to a fixed-opcode format (mostly due to having to use function pointers to swap out opcodes)
- Fast-path performance optimisations can be implemented for commonly used opcode combinations, allowing them to be just as performant as if used in a fixed-opcode format
- The format can be extended with more opcodes for better compression or to support different types of input (potentially higher bit depths, YCbCr, chroma subsampling, etc)

## What this is

This is simply a flexible QOI-like streaming format, meaning:
- Pixels are read only once
- Pixels are stored according to a known opcode
- Pixels are read in the order they are received
- Opcodes are defined in the header, fixed for the duration of the bitstream

## What this is not

- ⚠️ This is not QOI 2.0. Configurable opcodes introduce a good chunk of complexity that probably won't fit a consensus of what a QOI 2.0 should be. However, QOIPond could be used as a tool to test new op combinations that may inform the design of a potential QOI 2.0
- ⚠️ This is not a complex file format definition. Things like parallel processing, colourspace transformations and traversal order are out of the scope of the bitstream format so are unlikely to be worked on here
- ⚠️ This format may never be finalised. Once out of alpha some effort will be made to ensure that future decoders can decode old files and old decoders recognise when they cannot decode future files, but nothing is guaranteed

## Format

This is primarily a bitstream format, but there is a thin shim of a file format (QOIP) wrapping it. The focus is mostly on the bitstream format, the file format is merely a convenience:

```
qoip_file_header {
	char     magic[4];     // Magic bytes "qoip"
	uint8_t  channels;     // 3 = RGB, 4 = RGBA
	uint8_t  colorspace;   // 0 = sRGB with linear alpha, 1 = all channels linear
	uint8_8  entropy;      // 0 = None, 1=LZ4, 2=ZSTD
	uint8_t  padding;      // Padded with 0x00 to 8 byte alignment
	uint64_t size;         // Size of the bitstream only (not including bitstream header)
	uint64_t entropy_size; // Only present if entropy coding is used. The size
                         // of the entropy-coded data
}

qoip_bitstream_header {
	uint32_t width;        // Image width in pixels
	uint32_t height;       // Image height in pixels
	uint8_t  version;      // Set to 0x00
	uint8_t  opcode_cnt;   // The number of opcodes used in this combination
	uint8_t  *opcodes;     // The opcodes used in ascending id order
	uint8_t  *padding;     // Padded with 0x00 to 8 byte alignment
}

qoip_bitstream {         // If entropy coding is used, this is what's encoded
	uint8_t *stream;       // The raw bitstream using a variable number of bytes.
	uint8_t *padding;      // 8-15 bytes of 0x00 padding to pad to 8 byte alignment
	                       // with at least 8 bytes of padding guaranteed
}

qoip_footer {
	uint8_t *padding;      // File padded with 0x00 to 8 byte alignment
}
```

## Limitations

- Opcodes OP_RGB and OP_RGBA are mandatory and implicit for all combinations as worst-case encodings
- OP_RUN2 is mandatory and implicit for all combinations
- OP_RUN1 is implicit, taking up all remaining opcodes after the explicit and mandatory ops have been assigned (meaning valid combinations always use all of the opcode space)
- Opcodes cannot have overlapping encodings
- When RLE can be used it must be used
- Opcodes are defined in the header and cannot be changed mid-bitstream
- Multiple 1 byte index encodings cannot be used simultaneously

## Code overview

### Main Library

- qoip.h - Main QOIP functions including the generic path implementations of encode/decode
- qoip-fast.c - Fastpath implementations for commonly used opcode combinations. The generic encode/decode in qoip.h uses a matching fastpath if available instead of the generic path
- qoip-func.c - Encode/decode functions for opcodes used by the generic path. Included by the QOIP_C implementation only, split from qoip.h to make it less unwieldy

### Crunch Library

- qoipcrunch.h - Crunch function wrapping qoip_encode, automating trying many combinations. Included by anything that wants to crunch
- qoipcrunch-list.h - A list of combinations tried by qoipcrunch_encode depending on effort level (higher effort levels try more combinations)

### Tools

- qoipbench - Commandline benchmark comparing QOIP, PNG and STBI formats
- qoipconv - Commandline converter to/from QOIP format
- qoipcrunch - Commandline crunch program, reduces size of a QOIP file by trying many opcode combinations
- qoipstat - Reads the header of a QOIP file to display information
- opt/* - Argument parsing code for the above tools

