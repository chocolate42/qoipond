/* QOIPond - Lossless image format inspired by QOI “Quite OK Image” format

Incompatible adaptation of QOI format - https://phoboslab.org

-- LICENSE: The MIT License(MIT)
Copyright(c) 2021 Dominic Szablewski (original QOI format)
Copyright(c) 2021 Matthew Ling (adaptations for QOIP format)

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
typedef   int8_t  i8;
typedef  uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;

#ifdef __cplusplus
extern "C" {
#endif

/* A pointer to a qoip_desc struct has to be supplied to all of qoip's functions.
It describes either the input format (for qoip_encode), or is
filled with the description read from the file header (for qoip_decode).

The colorspace in this qoip_desc is an enum where
	0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
	1 = all channels are linear
The colorspace is purely informative. It will be saved to the file header, but
does not affect en-/decoding in any way. */

enum{QOIP_SRGB, QOIP_LINEAR};

typedef struct {
	u32 width;
	u32 height;
	u8 channels;
	u8 colorspace;
	u64 encoded_size;
} qoip_desc;

/* Populate desc by reading a QOIP header. If loc is NULL, read from
bytes + 0, otherwise read from bytes + *loc. Advance loc if present. */
int qoip_read_header(const unsigned char *bytes, size_t *loc, qoip_desc *desc);

/* Return the maximum size of a QOIP encoded image with dimensions in desc */
size_t qoip_maxsize(const qoip_desc *desc);

/* Return the maximum size of a decoded image with dimensions in desc */
size_t qoip_maxsize_raw(const qoip_desc *desc, int channels);

/* Encode raw RGB or RGBA pixels into a QOIP image in memory. The function either
returns 1 on failure (invalid parameters) or 0 on success. On success out is the
encoded data, out_len is its size in bytes. out is assumed to be large enough to
hold the encoded data (see qoip_maxsize()). opcode_string is an optional string
defining the opcode combination to use. It is up to the caller to ensure this
string is valid, NULL is allowed which means the encoder uses the default combination. */
int qoip_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *opcode_string);

/* Decode a QOIP image from memory. The function either returns 1 on failure
(invalid parameters) or 0 on sucess. On success, the qoip_desc struct is filled
with the description from the file header. */
int qoip_decode(const void *data, size_t data_len, qoip_desc *desc, int channels, void *out);

/* Opcode id enum. Never change order, remove an op, or add a new op anywhere other
than at the end. Doing this allows for basic backwards compatibility.
The order of this enum must match the order of qoip_ops[] as these values are an
index into it. Less kludgey implementation TODO */
enum{
	OP_RGB, OP_RGBA, OP_A,
	OP_INDEX8, OP_INDEX7, OP_INDEX6, OP_INDEX5, OP_INDEX4, OP_INDEX3, OP_INDEX2,
	OP_DIFF, OP_LUMA2_464, OP_RGB3, OP_LUMA1_232, OP_LUMA3_676, OP_LUMA3_4645,
	/* new_op id goes here */
	OP_END
};

#ifdef __cplusplus
}
#endif
#endif /* QOIP_H */

#ifdef QOIP_C
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	size_t p, px_len;
	int channels, run, run1_len, run2_len, index1_maxval;
	unsigned char *out;
	const unsigned char *in;
	qoip_rgba_t index[128], index2[256], px, px_prev;
	i8 vr, vg, vb, va, vg_r, vg_b;
	u8 run1_opcode, run2_opcode;
} qoip_working_t;

/* All attributes necessary to use an opcode */
typedef struct {
	u8 id, mask, set, opcode, opcnt;
	int (*enc)(qoip_working_t *, u8);
	void (*dec)(qoip_working_t *);
} qoip_opcode_t;

/* Expanded generic definition of an opcode. Half of these values are copied
from global qoip_ops into the qoip_opcode_t used at runtime */
typedef struct {
	u8 id, mask, set;
	char *desc;
	int (*enc)(qoip_working_t *, u8);
	void (*dec)(qoip_working_t *);
	int opcnt;
} opdef_t;

/* Op encode/decode functions
qoip_enc_* is the encoder for OP_*, qoip_dec_* is the decoder.

For non-run ops the encode functions detect if an op can be used and encodes it
if it can. If the op is used 1 is returned so qoip_encode knows to proceed to the
next pixel. Run ops are special, detection is handled in qoip_encode so the encode
functions always encode.

The decode functions are called when qoip_decode has determined the op was used, no
detection necessary.
*/

static int qoip_enc_a(qoip_working_t *q, u8 opcode) {
	if ( q->vr == 0 && q->vg == 0 && q->vb == 0 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->px.rgba.a;
		return 1;
	}
	return 0;
}
static void qoip_dec_a(qoip_working_t *q) {
	++q->p;
	q->px.rgba.a = q->in[q->p++];
}

static int qoip_enc_diff(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vr > -3 && q->vr < 2 &&
		q->vg > -3 && q->vg < 2 &&
		q->vb > -3 && q->vb < 2
	) {
		q->out[q->p++] = opcode | (q->vr + 2) << 4 | (q->vg + 2) << 2 | (q->vb + 2);
		return 1;
	}
	return 0;
}
static void qoip_dec_diff(qoip_working_t *q) {
	q->px.rgba.r += ((q->in[q->p] >> 4) & 0x03) - 2;
	q->px.rgba.g += ((q->in[q->p] >> 2) & 0x03) - 2;
	q->px.rgba.b += ( q->in[q->p]       & 0x03) - 2;
	++q->p;
}

/* This function encodes all index1_* ops */
static int qoip_enc_index(qoip_working_t *q, u8 opcode) {
	int index_pos = QOIP_COLOR_HASH(q->px) & q->index1_maxval;
	if (q->index[index_pos].v == q->px.v) {
		q->out[q->p++] = opcode | index_pos;
		return 1;
	}
	q->index[index_pos] = q->px;
	return 0;
}
static void qoip_dec_index7(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x7f];
}
static void qoip_dec_index6(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x3f];
}
static void qoip_dec_index5(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x1f];
}
static void qoip_dec_index4(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x0f];
}
static void qoip_dec_index3(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x07];
}
static void qoip_dec_index2(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & 0x03];
}

static int qoip_enc_index8(qoip_working_t *q, u8 opcode) {
	int index_pos = QOIP_COLOR_HASH(q->px) & 0xff;
	if (q->index2[index_pos].v == q->px.v) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = index_pos;
		return 1;
	}
	q->index2[index_pos] = q->px;
	return 0;
}
static void qoip_dec_index8(qoip_working_t *q) {
	++q->p;
	q->px = q->index2[q->in[q->p++]];
}

static int qoip_enc_luma1_232(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r > -3 && q->vg_r < 2 &&
		q->vg > -5   && q->vg < 4 &&
		q->vg_b > -3 && q->vg_b < 2
	) {
		q->out[q->p++] = opcode | ((q->vg + 4) << 4) | ((q->vg_r + 2) << 2) | (q->vg_b + 2);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma1_232(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int vg = ((b1 >> 4) - 4);
	q->px.rgba.r += vg - 2 + ((b1 >> 2) & 0x03);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 2 + ((b1     ) & 0x03);
}

static int qoip_enc_luma2_464(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r >  -9 && q->vg_r <  8 &&
		q->vg   > -33 && q->vg   < 32 &&
		q->vg_b >  -9 && q->vg_b <  8
	) {
		q->out[q->p++] = opcode             | (q->vg   + 32);
		q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma2_464(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b1 & 0x3f) - 32;
	q->px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 8 +  (b2       & 0x0f);
}

static int qoip_enc_luma3_4645(qoip_working_t *q, u8 opcode) {
	if (
		q->va   > -17 && q->va   < 16 &&
		q->vg_r >  -9 && q->vg_r <  8 &&
		q->vg   > -33 && q->vg   < 32 &&
		q->vg_b >  -9 && q->vg_b <  8
	) {
		q->out[q->p++] = opcode      | ((q->vg   + 32) >> 3);
		q->out[q->p++] = (((q->vg + 32) & 0x07) << 5) | (q->va +  16);
		q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma3_4645(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 0x07) << 3) | (b2 >> 5)) - 32;
	q->px.rgba.r += vg - 8 + ((b3 >> 4) & 0x0f);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 8 +  (b3       & 0x0f);
	q->px.rgba.a += (b2 & 0x1f) - 16;
}

static int qoip_enc_luma3_676(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r > -33 && q->vg_r < 32 &&
		q->vg   > -65 && q->vg   < 64 &&
		q->vg_b > -33 && q->vg_b < 32
	) {
		q->out[q->p++] = opcode  | ((q->vg + 64) >> 4);
		q->out[q->p++] = (((q->vg   + 64) & 0x0f) << 4) | ((q->vg_b + 32) >> 2);
		q->out[q->p++] = (((q->vg_b + 32) & 0x03) << 6) | ( q->vg_r + 32      );
		return 1;
	}
	return 0;
}
static void qoip_dec_luma3_676(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 0x07) << 4) | (b2 >> 4)) - 64;
	q->px.rgba.r += vg - 32 + (b3 & 0x3f);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 32 + (((b2 & 0x0f) << 2) | ((b3 >> 6) & 0x03));
}

static int qoip_enc_rgb3(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vr >  -65 && q->vr <  64 &&
		q->vb >  -65 && q->vb <  64
	) {
		q->out[q->p++] = opcode                     | ((q->vr + 64) >> 1);
		q->out[q->p++] = q->px.rgba.g;
		q->out[q->p++] = (((q->vr + 64) & 1) << 7) | (q->vb + 64);
		return 1;
	}
	return 0;
}
static void qoip_dec_rgb3(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	q->px.rgba.r += (((b1 & 0x3f) << 1) | ((b3) >> 7)) - 64;
	q->px.rgba.g  = b2;
	q->px.rgba.b += (b3 & 0x7f) - 64;
}

static int qoip_enc_rgb(qoip_working_t *q, u8 opcode) {
	if ( q->va == 0 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->px.rgba.r;
		q->out[q->p++] = q->px.rgba.g;
		q->out[q->p++] = q->px.rgba.b;
		return 1;
	}
	return 0;
}
static void qoip_dec_rgb(qoip_working_t *q) {
	++q->p;
	q->px.rgba.r = q->in[q->p++];
	q->px.rgba.g = q->in[q->p++];
	q->px.rgba.b = q->in[q->p++];
}

static int qoip_enc_rgba(qoip_working_t *q, u8 opcode) {
	q->out[q->p++] = opcode;
	q->out[q->p++] = q->px.rgba.r;
	q->out[q->p++] = q->px.rgba.g;
	q->out[q->p++] = q->px.rgba.b;
	q->out[q->p++] = q->px.rgba.a;
	return 1;
}
static void qoip_dec_rgba(qoip_working_t *q) {
	++q->p;
	q->px.rgba.r = q->in[q->p++];
	q->px.rgba.g = q->in[q->p++];
	q->px.rgba.b = q->in[q->p++];
	q->px.rgba.a = q->in[q->p++];
}

/* new_op encode/decode functions go here */

/* For ease of implementation treat qoip_ops the same as the opcode enum.
Corresponding values must be in the same relative location */
static const opdef_t qoip_ops[] = {
	{OP_RGB,    MASK_8, QOIP_SET_LEN4, "OP_RGB:  4 byte, store RGB  verbatim", qoip_enc_rgb, qoip_dec_rgb, 1},
	{OP_RGBA,   MASK_8, QOIP_SET_LEN5, "OP_RGBA: 5 byte, store RGBA verbatim", qoip_enc_rgba, qoip_dec_rgba, 1},
	{OP_A,      MASK_8, QOIP_SET_LEN2, "OP_A:    2 byte, store    A verbatim", qoip_enc_a, qoip_dec_a, 1},
	{OP_INDEX8, MASK_8, QOIP_SET_INDEX2, "OP_INDEX8: 2 byte, 256 value index cache", qoip_enc_index8, qoip_dec_index8, 1},
	{OP_INDEX7, MASK_1, QOIP_SET_INDEX1, "OP_INDEX7: 1 byte, 128 value index cache", qoip_enc_index, qoip_dec_index7, 128},
	{OP_INDEX6, MASK_2, QOIP_SET_INDEX1, "OP_INDEX6: 1 byte,  64 value index cache", qoip_enc_index, qoip_dec_index6,  64},
	{OP_INDEX5, MASK_3, QOIP_SET_INDEX1, "OP_INDEX5: 1 byte,  32 value index cache", qoip_enc_index, qoip_dec_index5,  32},
	{OP_INDEX4, MASK_4, QOIP_SET_INDEX1, "OP_INDEX4: 1 byte,  16 value index cache", qoip_enc_index, qoip_dec_index4,  16},
	{OP_INDEX3, MASK_5, QOIP_SET_INDEX1, "OP_INDEX3: 1 byte,   8 value index cache", qoip_enc_index, qoip_dec_index3,   8},
	{OP_INDEX2, MASK_6, QOIP_SET_INDEX1, "OP_INDEX2: 1 byte,   4 value index cache", qoip_enc_index, qoip_dec_index2,   4},

	{OP_DIFF,       MASK_2, QOIP_SET_LEN1, "OP_DIFF:       1 byte delta,   vr  -2..1,  vg  -2..1,    vb  -2..1", qoip_enc_diff, qoip_dec_diff, 64},
	{OP_LUMA2_464,  MASK_2, QOIP_SET_LEN2, "OP_LUMA2_464:  2 byte delta, vg_r  -8..7,  vg -32..31, vg_b  -8..7", qoip_enc_luma2_464, qoip_dec_luma2_464, 64},
	{OP_RGB3,       MASK_2, QOIP_SET_LEN3, "OP_RGB3:       3 byte delta,   vr -64..63,  g,           vb -64..63", qoip_enc_rgb3, qoip_dec_rgb3, 64},
	{OP_LUMA1_232,  MASK_1, QOIP_SET_LEN1, "OP_LUMA1_232:  1 byte delta, vg_r  -2..1,  vg  -4..3,  vg_b  -2..1", qoip_enc_luma1_232, qoip_dec_luma1_232, 128},
	{OP_LUMA3_676,  MASK_5, QOIP_SET_LEN3, "OP_LUMA3_676:  3 byte delta, vg_r -32..31, vg -64..63, vg_b -32..31", qoip_enc_luma3_676, qoip_dec_luma3_676, 8},
	{OP_LUMA3_4645, MASK_5, QOIP_SET_LEN3, "OP_LUMA3_4645: 3 byte delta, vg_r  -8..7,  vg -32..31, vg_b  -8..7  va -16..15", qoip_enc_luma3_4645, qoip_dec_luma3_4645, 8},
	/* new_op definition goes here*/
	{OP_END}
};

void qoip_print_ops(const opdef_t *ops, FILE *io) {
	for(;ops->id!=OP_END;++ops)
		fprintf(io, "id=%02x, %s\n", ops->id, ops->desc);
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

static int qoip_read_file_header(const unsigned char *bytes, size_t *p, qoip_desc *desc) {
	size_t loc = p ? *p : 0;
	unsigned int header_magic = qoip_read_32(bytes, &loc);
	desc->channels = bytes[loc++];
	desc->colorspace = bytes[loc++];
	loc+=2;/*padding*/
	desc->encoded_size = qoip_read_64(bytes, &loc);
	if (p)
		*p = loc;
	return desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 || header_magic != QOIP_MAGIC;
}

static int qoip_read_bitstream_header(const unsigned char *bytes, size_t *p, qoip_desc *desc, qoip_opcode_t *ops, int *op_cnt) {
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

/* Convenience method for external code to populate desc without knowing internals */
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
	for(i=0;i<10;++i)
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
	for(;opstr[i];++i) {
		if(i==OP_END)
			return 1;/* More ops defined than exist in the implementation */
		num = qoip_valid_char(opstr[i]);
		if(num == -1)
			return 1;/* First char failed the number test */
		++i;
		if(qoip_valid_char(opstr[i]) == -1)
			return 1;/* Second char failed the number test */
		num = (num<<4) | qoip_valid_char(opstr[i]);
		if(num>=OP_END)
			return 1;/* An op is defined beyond the largest op in the implementation */
		ops[*op_cnt].id=num;
		++*op_cnt;
	}
	qsort(ops, *op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_id);
	for(i=1;i<*op_cnt;++i) {
		if(ops[i].id==ops[i-1].id)
			return 1;/* Repeated opcode */
	}
	for(i=0;i<*op_cnt;++i) {
		if(qoip_ops[ops[i].id].set == QOIP_SET_INDEX1)
			++index1_present;
	}
	if(index1_present>1)
		return 1;/* Multiple 1 byte run or index encodings, invalid combination */
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
		ops[i].opcnt  =  qoip_ops[ops[i].id].opcnt;
		if(ops[i].set==QOIP_SET_INDEX1)
			q->index1_maxval = ops[i].opcnt - 1;
	}

	qsort(ops, *op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_mask);
	for(i=0;i<*op_cnt;++i) {
		ops[i].opcode = op;
		op += ops[i].opcnt;
	}
	if(op>255)/* Too many ops */
		return 1;
	q->run2_len=256;
	q->run2_opcode = op;
	++op;
	q->run1_opcode=op;
	q->run1_len = 256 - q->run1_opcode;
	//if(q->run2_len!=-1) {/*run2 present, extend the number of storable values */
	//	q->run2_len += q->run1_len;
	//}
	return 0;
}

static inline void qoip_encode_run(qoip_working_t *q) {
	for(; q->run>=256; q->run-=256) {
		q->out[q->p++] = q->run2_opcode;
		q->out[q->p++] = 255;
	}
	if(q->run>q->run1_len) {
		q->out[q->p++] = q->run2_opcode;
		q->out[q->p++] = q->run - 1;
		q->run = 0;
	}
	else if(q->run) {
		q->out[q->p++] = q->run1_opcode + (q->run - 1);
		q->run = 0;
	}
}

static inline void qoip_encode_inner(qoip_working_t *q, qoip_opcode_t *op, int op_cnt) {
	int i;
	if (q->px.v == q->px_prev.v)
		++q->run;/* Accumulate as much RLE as there is */
	else {
		qoip_encode_run(q);
		/* generate variables that may be needed by ops */
		q->vr = q->px.rgba.r - q->px_prev.rgba.r;
		q->vg = q->px.rgba.g - q->px_prev.rgba.g;
		q->vb = q->px.rgba.b - q->px_prev.rgba.b;
		q->va = q->px.rgba.a - q->px_prev.rgba.a;
		q->vg_r = q->vr - q->vg;
		q->vg_b = q->vb - q->vg;
		/* Test every op until we find one that handles the pixel */
		for(i=0;i<op_cnt;++i){
			if(op[i].enc(q, op[i].opcode))
				break;
		}
	}
	q->px_prev = q->px;
}

static int qoip_ret(int ret, FILE *io, char *s) {
	fprintf(io, "%s\n", s);
	return ret;
}

int qoip_opstring_comp_id(const void *a, const void *b) {
	return memcmp(a, b, 2);
}

static void qoip_finish(qoip_working_t *q) {
	/* Write bitstream size to file header, a streaming version would skip this step */
	qoip_write_64(q->out+8, q->p-16);

	/* Pad footer to 8 byte alignment with minimum 8 bytes of padding */
	for(;q->p%8;)
		q->out[q->p++] = 0;
	q->out[q->p++] = 0;
	for(;q->p%8;)
		q->out[q->p++] = 0;
}

/* fastpath definitions */
#include "qoip-fast.c"
typedef struct {
	char *opstr;
	int (*enc)(qoip_working_t*, size_t*);
	int (*dec)(qoip_working_t*, size_t);
} qoip_fastpath_t;

int qoip_fastpath_cnt = 0;/*disable for run1 rework until implemented TODO*/
static const qoip_fastpath_t qoip_fastpath[] = {
	{"00010203060e121314", qoip_encode_default, qoip_decode_default},
};

int qoip_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *opstring) {
	char opstr[513];
	size_t px_pos, opstr_len;
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

	if(opstring == NULL || *opstring==0)
		opstring = "000102060a0b0c";/* Default, propA */
	if((opstr_len=strlen(opstring))%2)
		return qoip_ret(1, stderr, "qoip_encode: Opstring invalid, must be multiple of two");
	if(opstr_len>512)
		return qoip_ret(1, stderr, "qoip_encode: Opstring invalid, too big");
	strcpy(opstr, opstring);
	for(i=0;i<opstr_len;++i)/*lowercase*/
		opstr[i] += (opstr[i]>64 && opstr[i]<71) ? 32 : 0;
	qsort(opstr, opstr_len/2, 2, qoip_opstring_comp_id);
	if(parse_opstring(opstr, ops, &op_cnt))
		return qoip_ret(1, stderr, "qoip_encode: Failed to parse opstring");
	/* Write header before run2 is potentially extracted from ops */
	qoip_write_file_header(q->out, &(q->p), desc);
	qoip_write_bitstream_header(q->out, &q->p, desc, ops, op_cnt);
	if(qoip_expand_opcodes(&op_cnt, ops, q))
		return qoip_ret(1, stderr, "qoip_encode: Failed to expand opstring");

	q->px_prev.v = 0;
	q->px_prev.rgba.a = 255;
	q->px = q->px_prev;
	q->px_len = desc->width * desc->height * desc->channels;
	q->channels = desc->channels;

	for(i=0;i<qoip_fastpath_cnt;++i) {/* Check for fastpath implementation */
		if(strcmp(opstr, qoip_fastpath[i].opstr)==0 && qoip_fastpath[i].enc)
			return qoip_fastpath[i].enc(q, out_len);
	}

	/* Sort ops into order they should be tested on encode */
	qsort(ops, op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_set);
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			q->px = *(qoip_rgba_t *)(q->in + px_pos);
			qoip_encode_inner(q, ops, op_cnt);
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			q->px.rgba.r = q->in[px_pos + 0];
			q->px.rgba.g = q->in[px_pos + 1];
			q->px.rgba.b = q->in[px_pos + 2];
			qoip_encode_inner(q, ops, op_cnt);
		}
	}
	qoip_encode_run(q);/* Cap off ending run if present*/
	qoip_finish(q);
	*out_len = q->p;
	return 0;
}

int qoip_decode(const void *data, size_t data_len, qoip_desc *desc, int channels, void *out) {
	char opstr[513] = {0};
	int i, op_cnt;
	size_t px_pos;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	qoip_opcode_t ops[OP_END];

	q->in = (const unsigned char *)data;
	q->out = (unsigned char *)out;
	q->channels = channels==0 ? desc->channels : channels;

	if (
		data == NULL || desc == NULL ||
		(q->channels != 3 && q->channels != 4) ||
		data_len < QOIP_FILE_HEADER_SIZE
	)
		return qoip_ret(1, stderr, "qoip_decode: Bad arguments");

	if(qoip_read_file_header(q->in, &(q->p), desc))
		return qoip_ret(1, stderr, "qoip_decode: Failed to read file header");
	if(qoip_read_bitstream_header(q->in, &(q->p), desc, ops, &op_cnt))
		return qoip_ret(1, stderr, "qoip_decode: Failed to read bitstream header");
	q->px_len = desc->width * desc->height * q->channels;
	q->px.v = 0;
	q->px.rgba.a = 255;

	for(i=0;i<op_cnt;++i)
		sprintf(opstr+(2*i), "%02x", ops[i].id);
	for(i=0;i<qoip_fastpath_cnt;++i) {/* Check for fastpath implementation */
		if(strcmp(opstr, qoip_fastpath[i].opstr)==0 && qoip_fastpath[i].dec)
			return qoip_fastpath[i].dec(q, data_len);
	}

	if(qoip_expand_opcodes(&op_cnt, ops, q))
		return qoip_ret(1, stderr, "qoip_decode: Failed to expand opstring");

	qsort(ops, op_cnt, sizeof(qoip_opcode_t), qoip_op_comp_mask);
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len) {
				if(q->in[q->p]==q->run2_opcode) {
					++q->p;
					q->run = q->in[q->p++];
				}
				else if(q->in[q->p]>q->run2_opcode)
					q->run = q->in[q->p++] - q->run1_opcode;
				else {
					for(i=0;i<op_cnt;++i) {
						if ((q->in[q->p] & ops[i].mask) == ops[i].opcode) {
							ops[i].dec(q);
							break;
						}
					}
				}
				q->index[QOIP_COLOR_HASH(q->px) & q->index1_maxval] = q->px;
			}
			*(qoip_rgba_t*)(q->out + px_pos) = q->px;
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len) {
				if(q->in[q->p]==q->run2_opcode) {
					++q->p;
					q->run = q->in[q->p++];
				}
				else if(q->in[q->p]>q->run2_opcode)
					q->run = q->in[q->p++] - q->run1_opcode;
				else {
					for(i=0;i<op_cnt;++i) {
						if ((q->in[q->p] & ops[i].mask) == ops[i].opcode) {
							ops[i].dec(q);
							break;
						}
					}
				}
				q->index[QOIP_COLOR_HASH(q->px) & q->index1_maxval] = q->px;
			}
			q->out[px_pos + 0] = q->px.rgba.r;
			q->out[px_pos + 1] = q->px.rgba.g;
			q->out[px_pos + 2] = q->px.rgba.b;
		}
	}
	return 0;
}

#endif /* QOIP_C */
