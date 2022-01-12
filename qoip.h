/* QOIPond - Lossless image format inspired by QOI “Quite OK Image” format

Incompatible adaptation of QOI format - https://phoboslab.org

-- LICENSE: The MIT License(MIT)
Copyright(c) 2021 Dominic Szablewski (QOI format QOIP is based on)
Copyright(c) 2021 Matthew Ling (QOIP format)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-- USAGE:
Define `QOIP_C` in *one* C file to create the implementation.

#define QOIP_C
#include "qoip.h"

Core functions:
This library provides the following functions
	qoip_encode: Encode raw RGB/RGBA pixels in memory to QOIP image in memory
	qoip_decode: Decode QOIP image in memory to raw RGB/RGBA pixels in memory

	qoip_encode can take an optional string defining a combination of opcodes to use.
	If set to NULL a default string is used.

	qoip_decode takes the number of channels to output (3 or 4), regardless of the
	number of channels the file contains.

-- FORMAT:
See README.md for layout.

The decoder and encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous
pixel value. Pixels are encoded in one of the used opcodes

The color channels are assumed to not be premultiplied with the alpha channel
("un-premultiplied alpha").

Each chunk starts with a 1..8 bit tag, followed by a number of data bits. The
bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned. All
values encoded in these data bits have the most significant bit on the left.

The 8-bit tags have precedence over other tags. A decoder must check for the
presence of an 8-bit tag first.

Once all pixels are accounted for we are done. There is padding to ensure that
unaligned 8 byte reads won't overrun (8-15 bytes of zeroes).

Implementing new ops tl;dr:
* Add op definition to id enum and qoip_ops
* Implement encode and decode functions
* Search for new_op in this source to find the appropriate locations
* qoipcrunch maintains an independant understanding of ops/sets in qoipcrunch.c
  that will also need updating
*/

#ifndef QOIP_H
#define QOIP_H

#define QOIP_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)
#define QOIP_MAGIC (((u32)'p') << 24 | ((u32)'i') << 16 | ((u32)'o') <<  8 | ((u32)'q'))
#define QOIP_FILE_HEADER_SIZE 16
#define QOIP_BITSTREAM_HEADER_MAXSIZE (16 + 256)

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
typedef   int8_t  i8;
typedef  uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;

#ifdef __cplusplus
extern "C" {
#endif

/* A pointer to a qoip_desc struct has to be supplied to all of qoip's functions.
It describes either the input format (for qoip_encode), or is
filled with the description read from the file header (for qoip_decode).*/

/* The colorspace in this qoip_desc is an enum where
	0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
	1 = all channels are linear
The colorspace is purely informative. It will be saved to the file header, but
does not affect en-/decoding in any way. */
enum{QOIP_SRGB, QOIP_LINEAR};

enum{QOIP_ENTROPY_NONE, QOIP_ENTROPY_LZ4, QOIP_ENTROPY_ZSTD};

/* Opcode id's. Never change order, remove an op, or add a new op anywhere other
than at the end. Doing this allows for basic backwards compatibility. The order
of this enum must match the order of qoip_ops[] as these values are an index
into it. Less kludgey implementation TODO */
enum{
	OP_INDEX8, OP_INDEX7, OP_INDEX6, OP_INDEX5, OP_INDEX4, OP_INDEX3,

	/* 1 byte RGB  ops */OP_DELTA, OP_DIFF1_222, OP_LUMA1_232,
	/* 2 byte RGB  ops */OP_LUMA2_454, OP_LUMA2_464,
	/* 3 byte RGB  ops */OP_LUMA3_676, OP_LUMA3_686, OP_LUMA3_787,
	/* 1 byte RGBA ops */OP_DELTAA,
	/* 2 byte RGBA ops */OP_A, OP_LUMA2_3433,
	/* 3 byte RGBA ops */OP_LUMA3_4645, OP_LUMA3_5654,
	/* 4 byte RGBA ops */OP_LUMA4_7777,
	OP_LUMA1_232_BIAS,
	/* new_op id goes here */
	OP_END
};

typedef struct {
	u32 width;
	u32 height;
	u8 channels;
	u8 colorspace;
	u64 raw_cnt;
	u64 entropy_cnt;
	int entropy;
} qoip_desc;

/* Decode a QOIP image from memory. The function either returns 1 on failure
(invalid parameters) or 0 on sucess. On success, the qoip_desc struct is filled
with the description from the file header. */
int qoip_decode(const void *data, size_t data_len, qoip_desc *desc, int channels, void *out, void *scratch);

/* Encode raw RGB or RGBA pixels into a QOIP image in memory. The function either
returns 1 on failure (invalid parameters) or 0 on success. On success out is the
encoded data, out_len is its size in bytes. out is assumed to be large enough to
hold the encoded data (see qoip_maxsize()). opcode_string is an optional string
defining the opcode combination to use. It is up to the caller to ensure this
string is valid, NULL is allowed which means the encoder uses the default combination. */
int qoip_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *opcode_string, int entropy, void *scratch);

/* Return the maximum size of a no-entropy-coding QOIP image with dimensions in desc */
size_t qoip_maxsize(const qoip_desc *desc);

/* Return the maximum size of a decoded image with dimensions in desc */
size_t qoip_maxsize_raw(const qoip_desc *desc, int channels);

/* Maximum size of an entropy-coded chunk of data, for implementation usage */
size_t qoip_maxentropysize(size_t src, int entropy);

/* Bolted-on entropy coding, exposed so qoipcrunch_encode can use it */
static int qoip_entropy(void *out, size_t *out_len, void *tmp, int entropy);

/* Populate desc by reading a QOIP header. If loc is NULL, read from
bytes + 0, otherwise read from bytes + *loc. Advance loc if present.
Convenience method for external code to read entire header without knowing internals */
int qoip_read_header(const unsigned char *bytes, size_t *loc, qoip_desc *desc);

/* File header read separately */
int qoip_read_file_header(const unsigned char *bytes, size_t *p, qoip_desc *desc);

/* Bitstream header read separately, just enough to determine its size.
This exists to avoid exposing internals with qoip_read_bitstream_header */
int qoip_skip_bitstream_header(const unsigned char *bytes, size_t *p, qoip_desc *desc);

/* Print s to io and return ret, typically used to print error and return error code */
int qoip_ret(int ret, FILE *io, char *s);

/* Print opcode layout from a QOIP header */
int qoip_stat(const void *encoded, FILE *io);

#ifdef __cplusplus
}
#endif
#endif /* QOIP_H */

#ifdef QOIP_C
#include <stdlib.h>
#include <stdio.h>
#include "lz4.h"
#include "zstd.h"

/* Defines which set an op belongs to. The order of this enum determines
the order ops are sorted for encode/decode. Modify with caution. */
enum{QOIP_SET_INDEX1, QOIP_SET_LEN1, QOIP_SET_INDEX2, QOIP_SET_LEN2, QOIP_SET_LEN3, QOIP_SET_LEN4, QOIP_SET_LEN5};

/* Decode masks */
enum{MASK_1=0x80, MASK_2=0xc0, MASK_3=0xe0, MASK_4=0xf0, MASK_5=0xf8, MASK_6=0xfc, MASK_7=0xfe, MASK_8=0xff};

typedef union {
	struct { u8 r, g, b, a; } rgba;
	u32 v;
} qoip_rgba_t;

/* All working variables needed by a single encode/decode run */
typedef struct {
	size_t p, px_pos, px_w, px_h, width, height, bitstream_loc;
	int channels, hash, run, run1_len, run2_len, index1_maxval, stride;
	unsigned char *out, upcache[8192*3];
	const unsigned char *in;
	qoip_rgba_t index[128], index2[256], px, px_prev, px_ref;
	i8 vr, vg, vb, va;/*Difference from previous */
	i8 avg_r, avg_g, avg_b, avg_gr, avg_gb;/*Difference from average*/
	u8 run1_opcode, run2_opcode, rgb_opcode, rgba_opcode;
} qoip_working_t;

/* All attributes necessary to use an opcode */
typedef struct {
	u8 id, mask, set, opcode, opcnt;
	int (*enc)(qoip_working_t *, u8);
	void (*dec)(qoip_working_t *);
	int (*sim)(qoip_working_t *);
} qoip_opcode_t;

/* Expanded generic definition of an opcode. Half of these values are copied
from global qoip_ops into the qoip_opcode_t used at runtime */
typedef struct {
	u8 id, mask, set;
	char *desc;
	int (*enc)(qoip_working_t *, u8);
	void (*dec)(qoip_working_t *);
	int (*sim)(qoip_working_t *);
	int opcnt;
} opdef_t;

/* Op encode/decode functions split into qoip-func.c, new_op functions go there */
#include "qoip-func.c"

/* For ease of implementation treat qoip_ops the same as the opcode enum.
Corresponding values must be in the same relative location */
static const opdef_t qoip_ops[] = {
	{OP_INDEX8, MASK_8, QOIP_SET_INDEX2, "OP_INDEX8: 2 byte, 256 value index cache", qoip_enc_index8, qoip_dec_index8, qoip_sim_index8, 1},
	{OP_INDEX7, MASK_1, QOIP_SET_INDEX1, "OP_INDEX7: 1 byte, 128 value index cache", qoip_enc_index, qoip_dec_index, qoip_sim_index, 128},
	{OP_INDEX6, MASK_2, QOIP_SET_INDEX1, "OP_INDEX6: 1 byte,  64 value index cache", qoip_enc_index, qoip_dec_index, qoip_sim_index,  64},
	{OP_INDEX5, MASK_3, QOIP_SET_INDEX1, "OP_INDEX5: 1 byte,  32 value index cache", qoip_enc_index, qoip_dec_index, qoip_sim_index,  32},
	{OP_INDEX4, MASK_4, QOIP_SET_INDEX1, "OP_INDEX4: 1 byte,  16 value index cache", qoip_enc_index, qoip_dec_index, qoip_sim_index,  16},
	{OP_INDEX3, MASK_5, QOIP_SET_INDEX1, "OP_INDEX3: 1 byte,   8 value index cache", qoip_enc_index, qoip_dec_index, qoip_sim_index,   8},

	{OP_DELTA,      MASK_3, QOIP_SET_LEN1, "OP_DELTA:      1 byte delta, ( avg_r  -1..1 , avg_g  -1..1 ,  avg_b  -1..1 ), AND\n"
                                         "                                              (            0 , avg_g  -4..3 ,             0 )", qoip_enc_delta, qoip_dec_delta, qoip_sim_delta, 32},
	{OP_DIFF1_222,  MASK_2, QOIP_SET_LEN1, "OP_DIFF:       1 byte delta, ( avg_r  -2..1 , avg_g  -2..1 ,   avg_b -2..1 )", qoip_enc_diff1_222, qoip_dec_diff1_222, qoip_sim_diff1_222, 64},
	{OP_LUMA1_232,  MASK_1, QOIP_SET_LEN1, "OP_LUMA1_232:  1 byte delta, (avg_gr  -2..1 , avg_g  -4..3 , avg_gb  -2..1 )", qoip_enc_luma1_232, qoip_dec_luma1_232, qoip_sim_luma1_232, 128},

	{OP_LUMA2_454,  MASK_3, QOIP_SET_LEN2, "OP_LUMA2_454:  2 byte delta, (avg_gr  -8..7 , avg_g -16..15, avg_gb  -8..7 )", qoip_enc_luma2_454, qoip_dec_luma2_454, qoip_sim_luma2_454, 32},
	{OP_LUMA2_464,  MASK_2, QOIP_SET_LEN2, "OP_LUMA2_464:  2 byte delta, (avg_gr  -8..7 , avg_g -32..31, avg_gb  -8..7 )", qoip_enc_luma2_464, qoip_dec_luma2_464, qoip_sim_luma2_464, 64},

	{OP_LUMA3_676,  MASK_5, QOIP_SET_LEN3, "OP_LUMA3_676:  3 byte delta, (avg_gr -32..31, avg_g -64..63, avg_gb -32..31)", qoip_enc_luma3_676, qoip_dec_luma3_676, qoip_sim_luma3_676, 8},
	{OP_LUMA3_686,  MASK_4, QOIP_SET_LEN3, "OP_LUMA3_686:  3 byte delta, (avg_gr -32..31, avg_g        , avg_gb -32..31)", qoip_enc_luma3_686, qoip_dec_luma3_686, qoip_sim_luma3_686, 16},
	{OP_LUMA3_787,  MASK_2, QOIP_SET_LEN3, "OP_LUMA3_787:  3 byte delta, (avg_gr -64..63, avg_g        , avg_gb -64..63)", qoip_enc_luma3_787, qoip_dec_luma3_787, qoip_sim_luma3_787, 64},

	{OP_DELTAA,     MASK_2, QOIP_SET_LEN1, "OP_DELTAA:     1 byte delta, ( avg_r  -1..1 , avg_g  -1..1 ,   avg_b -1..1 , va -1 or 1), AND\n"
                                         "                                              (            0 ,            0 ,              0, va  -5..4 )", qoip_enc_deltaa, qoip_dec_deltaa, qoip_sim_deltaa, 64},
	{OP_A,          MASK_8, QOIP_SET_LEN2, "OP_A:          2 byte delta, (            0 ,            0 ,             0 ,  a        )", qoip_enc_a, qoip_dec_a, qoip_sim_a, 1},
	{OP_LUMA2_3433, MASK_3, QOIP_SET_LEN2, "OP_LUMA2_3433: 2 byte delta, (avg_gr  -4..3 , avg_g  -8..7 , avg_gb  -4..3 , va  -4..3 )", qoip_enc_luma2_3433, qoip_dec_luma2_3433, qoip_sim_luma2_3433, 32},
	{OP_LUMA3_4645, MASK_5, QOIP_SET_LEN3, "OP_LUMA3_4645: 3 byte delta, (avg_gr  -8..7 , avg_g -32..31, avg_gb  -8..7 , va -16..15)", qoip_enc_luma3_4645, qoip_dec_luma3_4645, qoip_sim_luma3_4645, 8},
	{OP_LUMA3_5654, MASK_4, QOIP_SET_LEN3, "OP_LUMA3_5654: 3 byte delta, (avg_gr -16..15, avg_g -32..31, avg_gb -16..15, va  -8..7 )", qoip_enc_luma3_5654, qoip_dec_luma3_5654, qoip_sim_luma3_5654, 16},
	{OP_LUMA4_7777, MASK_4, QOIP_SET_LEN4, "OP_LUMA4_7777: 4 byte delta, (avg_gr -64..63, avg_g -64..63, avg_gb -64..63, va -64..63)", qoip_enc_luma4_7777, qoip_dec_luma4_7777, qoip_sim_luma4_7777, 16},


	{OP_LUMA1_232_BIAS,  MASK_1, QOIP_SET_LEN1, "OP_LUMA1_232B: 1 byte delta, OP_LUMA1_232 but with R and B biased depending on direction of G", qoip_enc_luma1_232_bias, qoip_dec_luma1_232_bias, qoip_sim_luma1_232_bias, 128},
	/* new_op definitions go here*/
	{OP_END},
};

void qoip_print_ops(const opdef_t *ops, FILE *io) {
	for(;ops->id!=OP_END;++ops)
		fprintf(io, "id=%02x, size=%3d, %s\n", ops->id, ops->opcnt, ops->desc);
}

void qoip_print_op(const opdef_t *op, FILE *io) {
	fprintf(io, "id=%02x, %s\n", op->id, op->desc);
}

/* Order by id for writing to header */
static int qoip_op_comp_id(const void *a, const void *b) {
	if( ((qoip_opcode_t *)a)->id == ((qoip_opcode_t *)b)->id )
		return 0;
	else
		return ( ((qoip_opcode_t *)a)->id < ((qoip_opcode_t *)b)->id ) ? -1: 1;
}

/* Order by mask size for opcode generation */
static int qoip_op_comp_mask(const void *a, const void *b) {
	if( ((qoip_opcode_t *)a)->mask == ((qoip_opcode_t *)b)->mask )
		return 0;
	else
		return ( ((qoip_opcode_t *)a)->mask < ((qoip_opcode_t *)b)->mask ) ? -1: 1;
}

/* Order to be tried on encode */
static int qoip_op_comp_set(const void *a, const void *b) {
	if( ((qoip_opcode_t *)a)->set == ((qoip_opcode_t *)b)->set )
		return 0;
	else
		return ( ((qoip_opcode_t *)a)->set < ((qoip_opcode_t *)b)->set ) ? -1: 1;
}

static u32 qoip_read_32(const unsigned char *bytes, size_t *p) {
	u32 r = bytes[(*p)++];
	r |= (bytes[(*p)++] <<  8);
	r |= (bytes[(*p)++] << 16);
	r |= (bytes[(*p)++] << 24);
	return r;
}

static u64 qoip_read_64(const unsigned char *bytes, size_t *p) {
	u64 r = bytes[(*p)++];
	r |= (bytes[(*p)++] <<  8);
	r |= (bytes[(*p)++] << 16);
	r |= (bytes[(*p)++] << 24);
	r |= ((u64)bytes[(*p)++] << 32);
	r |= ((u64)bytes[(*p)++] << 40);
	r |= ((u64)bytes[(*p)++] << 48);
	r |= ((u64)bytes[(*p)++] << 56);
	return r;
}

int qoip_read_file_header(const unsigned char *bytes, size_t *p, qoip_desc *desc) {
	size_t loc = p ? *p : 0;
	unsigned int header_magic = qoip_read_32(bytes, &loc);
	desc->channels = bytes[loc++];
	desc->colorspace = bytes[loc++];
	desc->entropy = bytes[loc++];
	++loc;/*padding*/
	desc->raw_cnt = qoip_read_64(bytes, &loc);
	desc->entropy_cnt = desc->entropy ? qoip_read_64(bytes, &loc) : 0;
	if (p)
		*p = loc;
	return desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 || header_magic != QOIP_MAGIC;
}

int qoip_skip_bitstream_header(const unsigned char *bytes, size_t *p, qoip_desc *desc) {
	u8 version, cnt;
	size_t loc = p ? *p : 0;
	desc->width  = qoip_read_32(bytes, &loc);
	desc->height = qoip_read_32(bytes, &loc);
	version = bytes[loc++];
	cnt = bytes[loc++];
	loc+=cnt;
	for(;loc%8;++loc) {
		if(bytes[loc])/* Padding non-zero */
			return 1;
	}

	if (p)
		*p = loc;
	return desc->width == 0 || desc->height == 0 || version || cnt == 0;
}

int qoip_read_bitstream_header(const unsigned char *bytes, size_t *p, qoip_desc *desc, qoip_opcode_t *ops, int *op_cnt) {
	int i;
	u8 version, cnt;
	size_t loc = p ? *p : 0;
	desc->width  = qoip_read_32(bytes, &loc);
	desc->height = qoip_read_32(bytes, &loc);
	version = bytes[loc++];
	cnt = bytes[loc++];
	if(op_cnt)
		*op_cnt = cnt;
	if(ops) {
		for(i=0; i<cnt; ++i)
			ops[i].id = bytes[loc++];
	}
	else
		loc+=cnt;
	for(;loc%8;++loc) {
		if(bytes[loc])/* Padding non-zero */
			return 1;
	}

	if (p)
		*p = loc;
	return desc->width == 0 || desc->height == 0 || version || cnt == 0;
}

int qoip_read_header(const unsigned char *bytes, size_t *p, qoip_desc *desc) {
	size_t loc = p ? *p : 0;
	if(qoip_read_file_header(bytes, &loc, desc))
		return 1;
	if(qoip_read_bitstream_header(bytes, &loc, desc, NULL, NULL))
		return 1;
	if (p)
		*p = loc;
	return 0;
}

static void qoip_write_32(unsigned char *bytes, size_t *p, u32 v) {
	bytes[(*p)++] = (v      ) & 0xff;
	bytes[(*p)++] = (v >>  8) & 0xff;
	bytes[(*p)++] = (v >> 16) & 0xff;
	bytes[(*p)++] = (v >> 24) & 0xff;
}

static void qoip_write_64(unsigned char *bytes, u64 v) {
	bytes[0] = (v      ) & 0xff;
	bytes[1] = (v >>  8) & 0xff;
	bytes[2] = (v >> 16) & 0xff;
	bytes[3] = (v >> 24) & 0xff;
	bytes[4] = (v >> 32) & 0xff;
	bytes[5] = (v >> 40) & 0xff;
	bytes[6] = (v >> 48) & 0xff;
	bytes[7] = (v >> 56) & 0xff;
}

void qoip_write_file_header(unsigned char *bytes, size_t *p, const qoip_desc *desc) {
	int i;
	qoip_write_32(bytes, p, QOIP_MAGIC);
	bytes[(*p)++] = desc->channels;
	bytes[(*p)++] = desc->colorspace;
	bytes[(*p)++] = 0;/*entropy coding, 0 placeholder for this non-streaming implementation*/
	bytes[(*p)++] = 0;/*padding*/
	for(i=0;i<8;++i)/*size placeholder*/
		bytes[(*p)++] = 0;
}

void qoip_write_bitstream_header(unsigned char *bytes, size_t *p, const qoip_desc *desc, qoip_opcode_t *ops, u8 op_cnt) {
	int i;
	qoip_write_32(bytes, p, desc->width);
	qoip_write_32(bytes, p, desc->height);
	bytes[(*p)++] = 0;/* Version */
	bytes[(*p)++] = op_cnt;
	qsort(ops, op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_id);
	for(i=0;i<op_cnt;++i)
		bytes[(*p)++] = ops[i].id;
	for(i=op_cnt+2;i%8;++i)/* Padding */
		bytes[(*p)++] = 0;
}

size_t qoip_maxsize(const qoip_desc *desc) {
	size_t max_size;
	if( desc == NULL || desc->width == 0 || desc->height == 0 )
		return 0;
	max_size = desc->width * desc->height * (desc->channels + 1) + QOIP_FILE_HEADER_SIZE + QOIP_BITSTREAM_HEADER_MAXSIZE + 16/*footer*/;
	return max_size;
}

size_t qoip_maxsize_raw(const qoip_desc *desc, int channels) {
	size_t max_size;
	if(desc == NULL || (channels != 0 && channels != 3 && channels != 4))
		return 0;
	max_size = desc->width * desc->height * (channels == 0 ? desc->channels : channels);
	return max_size;
}

/* Parse an ascii char as a hex value, return -1 on failure */
static inline int qoip_valid_char(u8 chr) {
	if(chr>='0' && chr<='9')
		return chr - '0';
	else if(chr>='a' && chr<='f')
		return 10 + chr - 'a';
	else if(chr>='A' && chr<='F')
		return 10 + chr - 'A';
	else
		return -1;
}

static int parse_opstring(char *opstr, qoip_opcode_t *ops, int *op_cnt) {
	int i=0, num, index1_present=0;
	*op_cnt = 0;
	for(; opstr[i] && opstr[i]!=','; ++i) {
		if(i==(2*OP_END))
			return 1;/* More ops defined than exist in the implementation */
		num = qoip_valid_char(opstr[i]);
		if(num == -1)
			return 2;/* First char failed the number test */
		++i;
		if(qoip_valid_char(opstr[i]) == -1)
			return 3;/* Second char failed the number test */
		num = (num<<4) | qoip_valid_char(opstr[i]);
		if(num>=OP_END)
			return 4;/* An op is defined beyond the largest op in the implementation */
		ops[*op_cnt].id=num;
		++*op_cnt;
	}
	qsort(ops, *op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_id);
	for(i=1;i<*op_cnt;++i) {
		if(ops[i].id==ops[i-1].id)
			return 5;/* Repeated opcode */
	}
	for(i=0;i<*op_cnt;++i) {
		if(qoip_ops[ops[i].id].set == QOIP_SET_INDEX1)
			++index1_present;
	}
	if(index1_present>1)
		return 6;/* Multiple 1 byte run or index encodings, invalid combination */
	return 0;
}

/* Generate everything related to an opcode, including the opcode itself */
static int qoip_expand_opcodes(int *op_cnt, qoip_opcode_t *ops, qoip_working_t *q) {
	int i, op = 0;
	for(i=0;i<*op_cnt;++i) {
		ops[i].mask = qoip_ops[ops[i].id].mask;
		ops[i].set  =  qoip_ops[ops[i].id].set;
		ops[i].enc  =  qoip_ops[ops[i].id].enc;
		ops[i].dec  =  qoip_ops[ops[i].id].dec;
		ops[i].sim  =  qoip_ops[ops[i].id].sim;
		ops[i].opcnt  =  qoip_ops[ops[i].id].opcnt;
		if(ops[i].set==QOIP_SET_INDEX1)
			q->index1_maxval = ops[i].opcnt - 1;
	}

	qsort(ops, *op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_mask);
	for(i=0;i<*op_cnt;++i) {
		ops[i].opcode = op;
		op += ops[i].opcnt;
	}
	if(op>253)/* Too many ops */
		return 1;
	q->rgb_opcode = op++;
	q->rgba_opcode = op++;
	q->run2_len=256;
	q->run2_opcode = op++;
	q->run1_opcode = op & 0xff;
	q->run1_len = (256 - q->run1_opcode) & 0xff;
	q->run2_len += q->run1_len;
	return 0;
}

static inline void qoip_encode_run(qoip_working_t *q) {
	for(; q->run>=q->run2_len; q->run-=q->run2_len) {
		q->out[q->p++] = q->run2_opcode;
		q->out[q->p++] = 255;
	}
	if(q->run>q->run1_len) {
		q->out[q->p++] = q->run2_opcode;
		q->out[q->p++] = (q->run - 1) - q->run1_len;
		q->run = 0;
	}
	else if(q->run) {
		q->out[q->p++] = q->run1_opcode + (q->run - 1);
		q->run = 0;
	}
}

int qoip_ret(int ret, FILE *io, char *s) {
	fprintf(io, "%s\n", s);
	return ret;
}

int qoip_opstring_comp_id(const void *aa, const void *bb) {
	unsigned char *a = (unsigned char *)aa;
	unsigned char *b = (unsigned char *)bb;
	if(a[0]==b[0]&&a[1]==b[1])
		return 0;
	else if (a[0]<b[0] || (a[0]==b[0] && a[1]<b[1]) )
		return -1;
	else
		return 1;
}

static void qoip_finish(qoip_working_t *q) {
	/* Pad footer to 8 byte alignment with minimum 8 bytes of padding */
	for(;q->p%8;)
		q->out[q->p++] = 0;
	q->out[q->p++] = 0;
	for(;q->p%8;)
		q->out[q->p++] = 0;

	/* Write bitstream size to file header, a streaming version might skip this step */
	qoip_write_64(q->out+8, q->p-q->bitstream_loc);
}

size_t qoip_maxentropysize(size_t src, int entropy) {
	switch(entropy) {
		case 0:
			return src;
		case 1:
			return LZ4_compressBound(src);
		case 2:
			return ZSTD_compressBound(src);
		default:
			return 0;
	}
}

/* Bolted-on entropy encoding implementation, this way it can be reused by
qoipcrunch_encode */
static int qoip_entropy(void *out, size_t *out_len, void *scratch, int entropy) {
	unsigned char *ptr = (unsigned char*)out;
	size_t p = 0, src_cnt, dst_cnt, loc_bithead, loc_bitstream;
	qoip_desc d;
	qoip_read_file_header(out, &p, &d);
	loc_bithead = p;
	qoip_skip_bitstream_header(out, &p, &d);
	loc_bitstream = p;
	src_cnt = *out_len - p;
	if(entropy==QOIP_ENTROPY_LZ4) {
		dst_cnt = LZ4_compress_default((char *)ptr+p, scratch, src_cnt, LZ4_compressBound(src_cnt));
		if(dst_cnt==0)
			return qoip_ret(1, stdout, "qoip_entropy: LZ4 compression failed\n");
	}
	else if(entropy==QOIP_ENTROPY_ZSTD) {
		dst_cnt = ZSTD_compress(scratch, ZSTD_compressBound(src_cnt), ptr+p, src_cnt, 19);
		if(ZSTD_isError(dst_cnt))
			return qoip_ret(1, stdout, "qoip_entropy: ZSTD compression failed\n");
	}
	else
		return qoip_ret(1, stdout, "qoip_entropy: Requested entropy coding unknown, update encoder?");

	if(dst_cnt<src_cnt) {
		for(p=loc_bitstream-1;p>=loc_bithead;--p)/*Shift bitstream header to make room for entropy_cnt*/
			ptr[p+8] = ptr[p];
		qoip_write_64(ptr+loc_bithead, dst_cnt);
		for(p=0;p<dst_cnt;++p)
			ptr[loc_bitstream + 8 + p] = ((unsigned char *)scratch)[p];
		p = loc_bitstream + 8 + dst_cnt;
		for(;p%8;)//EOF padding
			ptr[p++]=0;
		*out_len = p;
		ptr[6]=entropy;
	}
	return 0;
}

int qoip_stat(const void *encoded, FILE *io) {
	int op_cnt;
	qoip_desc desc;
	qoip_opcode_t ops[OP_END];
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	size_t i, p = 0, raw;
	const unsigned char *bytes = (const unsigned char *) encoded;

	if(qoip_read_file_header(bytes, &p, &desc))
		return qoip_ret(1, stderr, "qoip_stat: Failed to read file header");
	if(qoip_read_bitstream_header(bytes, &p, &desc, ops, &op_cnt))
		return qoip_ret(1, stderr, "qoip_stat: Failed to read bitstream header");
	if(qoip_expand_opcodes(&op_cnt, ops, q))
		return qoip_ret(1, stderr, "qoip_stat: Failed to expand opstring");

	fprintf(io, "Width: %6"PRIu32"\n", desc.width);
	fprintf(io, "Height:%6"PRIu32"\n", desc.height);
	fprintf(io, "Channels: %"PRIu8"\n", desc.channels);
	fprintf(io, "Colorspace: %s\n", desc.colorspace==QOIP_SRGB ? "sRGB" : "Linear");

	if(     desc.entropy==QOIP_ENTROPY_NONE)
		fprintf(io, "Entropy coding: None\n");
	else if(desc.entropy==QOIP_ENTROPY_LZ4)
		fprintf(io, "Entropy coding: LZ4\n");
	else if(desc.entropy==QOIP_ENTROPY_ZSTD)
		fprintf(io, "Entropy coding: ZSTD\n");
	else
		fprintf(io, "Entropy coding: Unknown\n");


	raw = desc.width*desc.height*desc.channels;
	fprintf(io,   "Raw size:                   %9zu\n", raw);
	if(desc.raw_cnt)
		fprintf(io, "Uncompressed bitstream size:%9"PRIu64"\n", desc.raw_cnt);
	else
		fprintf(io, "Uncompressed bitstream size unknown (streamed)\n");
	if(desc.entropy_cnt)
		fprintf(io, "Entropy-coded size:         %9"PRIu64"\n", desc.entropy_cnt);

	fprintf(io, "Opstring: ");
	for(i=0; i<op_cnt; ++i)
		fprintf(io, "%02x", ops[i].id);
	fprintf(io, "\n\n");

	fprintf(io, "Ops:\n");
	for(i=0; i<op_cnt; ++i)
		fprintf(io, "%s\n", qoip_ops[ops[i].id].desc);
	fprintf(io, "OP_RGB\n");
	fprintf(io, "OP_RGBA\n");
	fprintf(io, "OP_RUN2\n");
	if(q->run1_opcode) {
		fprintf(io, "OP_RUN1\n");
		fprintf(io, "RUN1 range: 1..%d\n", q->run1_len);
	}
	fprintf(io, "RUN2 range: %d..%d\n", q->run1_len+1, q->run2_len);
	return 0;
}

/* fastpath definitions */
#include "qoip-fast.c"
typedef struct {
	char *opstr;
	int (*enc)(qoip_working_t*, size_t*);
	int (*dec)(qoip_working_t*, size_t);
} qoip_fastpath_t;

int qoip_fastpath_cnt = 0;/*Disabled for average implementation as it breaks these TODO*/
static const qoip_fastpath_t qoip_fastpath[] = {
	{0}
	//{"0003080a0b11", qoip_encode_deltax, qoip_decode_deltax},
	//{"0001060a0f", qoip_encode_idelta, qoip_decode_idelta},
	//{"03070a0d0f", qoip_encode_propc, qoip_decode_propc},
};

static inline void qoip_encode_inner(qoip_working_t *q, qoip_opcode_t *op, int op_cnt) {
	int i;
	if (q->px.v == q->px_prev.v)
		++q->run;/* Accumulate as much RLE as there is */
	else {
		qoip_encode_run(q);
		/* generate variables that may be needed by ops */
		if (q->px_w<8192) {
			q->px_ref.rgba.r = (q->px_prev.rgba.r + q->upcache[(q->px_w * 3) + 0] + 1) >> 1;
			q->px_ref.rgba.g = (q->px_prev.rgba.g + q->upcache[(q->px_w * 3) + 1] + 1) >> 1;
			q->px_ref.rgba.b = (q->px_prev.rgba.b + q->upcache[(q->px_w * 3) + 2] + 1) >> 1;
		}
		else
			q->px_ref.v = q->px_prev.v;
		q->hash = QOIP_COLOR_HASH(q->px) & 255;
		q->vr = q->px.rgba.r - q->px_prev.rgba.r;
		q->vg = q->px.rgba.g - q->px_prev.rgba.g;
		q->vb = q->px.rgba.b - q->px_prev.rgba.b;
		q->va = q->px.rgba.a - q->px_prev.rgba.a;
		q->avg_r = q->px.rgba.r - q->px_ref.rgba.r;
		q->avg_g = q->px.rgba.g - q->px_ref.rgba.g;
		q->avg_b = q->px.rgba.b - q->px_ref.rgba.b;
		q->avg_gr = q->avg_r - q->avg_g;
		q->avg_gb = q->avg_b - q->avg_g;
		/* Test every op until we find one that handles the pixel */
		for(i=0;i<op_cnt;++i){
			if(op[i].enc(q, op[i].opcode))
				break;
		}
		if(i==op_cnt) {
			if(q->va==0) {
				q->out[q->p++] = q->rgb_opcode;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
			}
			else {
				q->out[q->p++] = q->rgba_opcode;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
				q->out[q->p++] = q->px.rgba.a;
			}
		}
	}
	if(q->px_w<8192) {
		q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
		q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
		q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
	}
	q->index2[q->hash] = q->px;
}

int qoip_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *opstring, int entropy, void *scratch) {
	char opstr[513] = {0};
	size_t opstr_len;
	int i, op_cnt = 0;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	qoip_opcode_t ops[OP_END];
	q->out = (unsigned char *) out;
	q->in = (const unsigned char *)data;

	if (
		data == NULL || desc == NULL || out == NULL || out_len == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1
	)
		return qoip_ret(1, stderr, "qoip_encode: Bad arguments");
	if (entropy && !scratch)
		return qoip_ret(1, stderr, "qoip_encode: Scratch space needs to be provided for entropy encoding");

	if(opstring == NULL || *opstring==0)
		opstring = "0003080a0b11";/* Default */
	for(opstr_len=0; opstring[opstr_len] && opstring[opstr_len]!=','; ++opstr_len);
	if(opstr_len%2)
		return qoip_ret(1, stderr, "qoip_encode: Opstring invalid, must be multiple of two");
	if(opstr_len>512)
		return qoip_ret(1, stderr, "qoip_encode: Opstring invalid, too big");
	for(i=0;i<opstr_len;++i)/*lowercase*/
		opstr[i] = opstring[i] + ((opstring[i]>64 && opstring[i]<71) ? 32 : 0);
	qsort(opstr, opstr_len/2, 2, qoip_opstring_comp_id);
	if(parse_opstring(opstr, ops, &op_cnt))
		return qoip_ret(1, stderr, "qoip_encode: Failed to parse opstring");
	if(qoip_expand_opcodes(&op_cnt, ops, q))
		return qoip_ret(1, stderr, "qoip_encode: Failed to expand opstring");
	qoip_write_file_header(q->out, &(q->p), desc);
	qoip_write_bitstream_header(q->out, &q->p, desc, ops, op_cnt);

	q->bitstream_loc = q->p;
	q->px.v = 0;
	q->px.rgba.a = 255;
	q->width = desc->width;
	q->height = desc->height;
	q->channels = desc->channels;
	q->stride = desc->width * desc->channels;
	q->upcache[0]=0;
	q->upcache[1]=0;
	q->upcache[2]=0;
	for(i=0;i<(desc->width<8192?desc->width-1:8191);++i) {/* Prefill upcache to remove branch */
		q->upcache[((i+1)*3)+0]=q->in[(i*desc->channels)+0];
		q->upcache[((i+1)*3)+1]=q->in[(i*desc->channels)+1];
		q->upcache[((i+1)*3)+2]=q->in[(i*desc->channels)+2];
	}

	/* Check for fastpath implementation */
	/*for(i=0;i<qoip_fastpath_cnt;++i) {
		if(strcmp(opstr, qoip_fastpath[i].opstr)==0 && qoip_fastpath[i].enc)
			return qoip_fastpath[i].enc(q, out_len);
	}*/

	/* Sort ops into order they should be tested on encode */
	qsort(ops, op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_set);
	q->px_pos = 0;
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px = *(qoip_rgba_t *)(q->in + q->px_pos);
				qoip_encode_inner(q, ops, op_cnt);
				q->px_pos +=4;
			}
		}
	}
	else {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px.rgba.r = q->in[q->px_pos + 0];
				q->px.rgba.g = q->in[q->px_pos + 1];
				q->px.rgba.b = q->in[q->px_pos + 2];
				qoip_encode_inner(q, ops, op_cnt);
				q->px_pos +=3;
			}
		}
	}
	qoip_encode_run(q);/* Cap off ending run if present*/
	qoip_finish(q);
	*out_len = q->p;

	if(entropy)
		qoip_entropy(out, out_len, scratch, entropy);
	return 0;
}

static inline void qoip_decode_inner(qoip_working_t *q, size_t data_len, qoip_opcode_t *op, int op_cnt) {
	int i;
	if (q->run > 0)
		--q->run;
	else if (q->p < data_len) {
		q->px_prev.v = q->px.v;
		if (q->px_pos >= q->stride && q->px_w<8192) {
			q->px_ref.rgba.r = (q->px.rgba.r + q->upcache[(q->px_w * 3) + 0] + 1) >> 1;
			q->px_ref.rgba.g = (q->px.rgba.g + q->upcache[(q->px_w * 3) + 1] + 1) >> 1;
			q->px_ref.rgba.b = (q->px.rgba.b + q->upcache[(q->px_w * 3) + 2] + 1) >> 1;
		}
		else
			q->px_ref.v = q->px_prev.v;
		if(q->in[q->p]==q->run2_opcode) {
			++q->p;
			q->run = q->in[q->p++] + q->run1_len;
		}
		else if(q->in[q->p]>q->run2_opcode)
			q->run = q->in[q->p++] - q->run1_opcode;
		else if(q->in[q->p]==q->rgb_opcode) {
			++q->p;
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
		}
		else if(q->in[q->p]==q->rgba_opcode) {
			++q->p;
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
			q->px.rgba.a = q->in[q->p++];
		}
		else {
			for(i=0;i<op_cnt;++i) {
				if ((q->in[q->p] & op[i].mask) == op[i].opcode) {
					op[i].dec(q);
					break;
				}
			}
		}
		q->index[QOIP_COLOR_HASH(q->px)  & q->index1_maxval] = q->px;
		q->index2[QOIP_COLOR_HASH(q->px) & 255] = q->px;
	}
	if(q->px_w<8192) {
		q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
		q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
		q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
	}
}

int qoip_decode(const void *data, size_t data_len, qoip_desc *desc, int channels, void *out, void *scratch) {
	char opstr[513] = {0};
	int i, op_cnt;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	qoip_opcode_t ops[OP_END];

	q->in = (const unsigned char *)data;
	q->out = (unsigned char *)out;

	if (
		data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		data_len < QOIP_FILE_HEADER_SIZE
	)
		return qoip_ret(1, stderr, "qoip_decode: Bad arguments");

	if(qoip_read_file_header(q->in, &(q->p), desc))
		return qoip_ret(1, stderr, "qoip_decode: Failed to read file header");
	if(qoip_read_bitstream_header(q->in, &(q->p), desc, ops, &op_cnt))
		return qoip_ret(1, stderr, "qoip_decode: Failed to read bitstream header");
	for(i=0;i<op_cnt;++i)
		sprintf(opstr+(2*i), "%02x", ops[i].id);
	if(qoip_expand_opcodes(&op_cnt, ops, q))
		return qoip_ret(1, stderr, "qoip_decode: Failed to expand opstring");

	if(desc->entropy) {
		if(!scratch)
			return qoip_ret(1, stderr, "qoip_decode: Scratch space needs to be provided for entropy decoding");
		if(desc->entropy==QOIP_ENTROPY_LZ4) {
			if(LZ4_decompress_safe((char *)q->in + q->p, (char *)scratch, desc->entropy_cnt, desc->raw_cnt)!=desc->raw_cnt)
				return qoip_ret(1, stderr, "qoip_decode: LZ4 decode failed");
		}
		else if(desc->entropy==QOIP_ENTROPY_ZSTD) {
			if(ZSTD_isError(ZSTD_decompress(scratch, desc->raw_cnt, q->in + q->p, desc->entropy_cnt)))
				return qoip_ret(1, stderr, "qoip_decode: ZSTD decode failed");
		}
		else
			return qoip_ret(1, stderr, "qoip_decode: Unknown entropy coding, update decoder?");
		q->p = 0;
		q->in = scratch;
	}

	q->width = desc->width;
	q->height = desc->height;
	q->channels = channels==0 ? desc->channels : channels;
	q->stride = desc->width * q->channels;
	q->px.v = 0;
	q->px.rgba.a = 255;

	/* Check for fastpath implementation */
	/*for(i=0;i<qoip_fastpath_cnt;++i) {
		if(strcmp(opstr, qoip_fastpath[i].opstr)==0 && qoip_fastpath[i].dec)
			return qoip_fastpath[i].dec(q, desc->entropy?desc->raw_cnt:data_len);
	}*/

	qsort(ops, op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_mask);
	q->px_pos = 0;
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_inner(q, desc->entropy?desc->raw_cnt:data_len, ops, op_cnt);
				*(qoip_rgba_t*)(q->out + q->px_pos) = q->px;
				q->px_pos += 4;
			}
		}
	}
	else {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_inner(q, desc->entropy?desc->raw_cnt:data_len, ops, op_cnt);
				q->out[q->px_pos + 0] = q->px.rgba.r;
				q->out[q->px_pos + 1] = q->px.rgba.g;
				q->out[q->px_pos + 2] = q->px.rgba.b;
				q->px_pos += 3;
			}
		}
	}
	return 0;
}

#endif /* QOIP_C */
