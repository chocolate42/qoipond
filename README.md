# QOIPond - Lossless image compression inspired by QOI “Quite OK Image” format

MIT licensed library for C/C++

⚠️ This should be in a functional state, but it's still an alpha WIP.

See [QOI](https://github.com/phoboslab/qoi) for the original format

## Why?

- QOI is fast and simple, but inflexible
- QOIPond builds on QOI by defining the set of opcodes used by a particular bitstream in the bitstream header

Flexibility like this means:
- The bitstream can be tailored to the input
- A size-optimising program (qoipcrunch) can try many combinations of opcode and pick the set that best represents a given input
- Caters to different use cases by allowing encode/decode/compression metrics to be balanced by the user
- General case performance takes a hit relative to a fixed-opcode format (mostly due to having to use function pointers to swap out opcodes)
- Fast-path performance optimisations can be implemented for commonly used opcode combinations, allowing them to be just as performant as if used in a fixed-opcode format (TODO)
- The format can be extended to more pixel formats like higher bit depths (TODO)

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
	char     magic[4];   // Magic bytes "qoip"
	uint8_t  channels;   // 3 = RGB, 4 = RGBA
	uint8_t  colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
	uint8_t  padding[2]; // Padded with 0x00 to 8 byte alignment
	uint64_t size;       // Optional size of the bitstream_header and bitstream combined,
                       // filled with 0x00 if unused
}

qoip_bitstream_header {
	uint32_t width;      // Image width in pixels
	uint32_t height;     // Image height in pixels
	uint8_t  version;    // Set to 0x00
	uint8_t  opcode_cnt; // The number of opcodes used in this combination
	uint8_t *opcodes;    // The opcodes used in ascending id order
	uint8_t *padding;    // Padded with 0x00 to 8 byte alignment
}

qoip_bitstream {
	uint8_t *stream;     // The raw bitstream using a variable number of bytes.
}

qoip_footer {
	uint8_t *padding;    // 8-15 bytes of 0x00 padding to pad to 8 byte alignment
	                     // with at least 8 bytes of padding guaranteed
}
```

## Limitations

- A 1 byte RLE op (RUN1, all RUN1_* ops) must exist (this could potentially be relaxed to any RLE encoding)
- Opcodes cannot have overlapping encodings, except 8-bit-tag ops possibly overlapping with the end of the RUN1 op (which is automatic if the tags have nowhere else to go)
- Opcodes used in a bitstream are defined in the header and cannot be changed mid-bitstream
- Multiple 1 byte RUN1 ops cannot be used simultaneously
- Multiple 1 byte index encodings cannot be used simultaneously

