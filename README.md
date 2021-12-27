# QOIPond - Lossless image compression inspired by QOI “Quite OK Image” format

MIT licensed library for C/C++

⚠️ This is currently a non-functional WIP, when this warning is removed it should be functional

See [QOI](https://github.com/phoboslab/qoi) for the original format

## Why?

- QOI is fast and simple, but inflexible
- QOIPond builds on QOI by defining the set of opcodes used by a particular bitstream in the bitstream's header

Flexibility like this means:
- The bitstream can be tailored to the input
- A potential size-optimising (crunch) program can try many combinations of opcode and pick the set that best represents a given input
- General case performance takes a hit relative to a fixed-opcode format (mostly due to having to use function pointers to swap out opcodes)
- Fast-path performance optimisations can be implemented for commonly used opcode combinations, allowing them to be just as performant as if used in a fixed-opcode format

## What this is

This is simply a flexible QOI-like streaming format, meaning:
- Pixels are read only once
- Pixels are stored according to a known opcode
- Pixels are read in the order they are received
- Opcodes are defined in the header, once defined they cannot be changed for the duration of the bitstream
-

## What this is not

- ⚠️ This is not QOI 2.0. Configurable opcodes introduce a good chunk of complexity that probably won't fit a consensus of what a QOI 2.0 should be. However, QOIPond could be used as a tool to test new op combinations that may inform the design of a potential QOI 2.0
- ⚠️ This is not a complex file format definition. Things like parallel processing, colourspace transformations and traversal order are out of the scope of the bitstream format so are unlikely to be worked on here
- ⚠️ This format may never be finalised. Some effort will be made to ensure that future decoders can decode old files, and that old decoders recognise when they cannot decode future files, but nothing is guaranteed

## Format

This is primarily a bitstream format, but there is a thin shim of a file format (QOIP) wrapping it. The focus is entirely on the bitstream format, the file format is merely a convenience:

- File header: "QOIP" magic word followed by a few bytes defining the state of the original image (channel count and colourspace at least to match QOI), and an optional filesize as u64le (zeroed if unused)
- Bitstream header: u32le width, u32le height, u8 version 0x00 in the unlikely event the header format needs to change, u8 opcode_count, u8[] ordered list of opcodes used, padded to 8 byte alignment with zeroes
- Bitstream, byte aligned
- Footer, zeroes to pad file to 8 byte alignment, minimum 8 bytes of padding maximum 15

## Limitations

- Opcodes cannot have overlapping encodings, except RUN1 which can house 8 bit opcodes at the end
- Opcodes used in a bitstream are defined in the header and cannot be changed mid-bitstream
- Multiple 1 byte RLE cannot be used simultaneously
- Multiple 1 byte index encodings cannot be used simultaneously
- A 1 byte RLE must exist (this could potentially be relaxed to any RLE encoding in the future)

