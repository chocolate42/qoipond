/* SPDX-License-Identifier: MIT */
/* Op encode/decode functions used by qoip.h implementation. Included by QOIP_C only

Copyright 2021 Matthew Ling

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

* qoip_enc_* is the encoder for OP_*, qoip_dec_* is the decoder
* The encode functions detect if an op can be used and encodes it if it can. If
  the op is used 1 is returned so qoip_encode knows to proceed to the next pixel
* The decode functions are called when qoip_decode has determined the op was
  used, no detection necessary
*/

/* === Hash cache index functions */
/* This function encodes all index1_* ops */
int qoip_enc_index(qoip_working_t *q, u8 opcode) {
	int index_pos = q->hash & q->index1_maxval;
	if (q->index[index_pos].v == q->px.v) {
		q->out[q->p++] = opcode | index_pos;
		return 1;
	}
	q->index[index_pos] = q->px;
	return 0;
}
void qoip_dec_index(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & q->index1_maxval];
}

int qoip_enc_indexf(qoip_working_t *q, u8 opcode) {
	if (q->index[q->hash_pos[q->hash & (QOIP_FIFO_HASH_SIZE - 1)] & q->index1_maxval].v == q->px.v) {
		q->out[q->p++] = opcode | (q->hash_pos[q->hash & (QOIP_FIFO_HASH_SIZE - 1)] & q->index1_maxval);
		return 1;
	}
	q->hash_pos[q->hash & (QOIP_FIFO_HASH_SIZE - 1)] = q->index_wpos;
	q->index[q->index_wpos++ & q->index1_maxval] = q->px;
	return 0;
}

int qoip_enc_index8(qoip_working_t *q, u8 opcode) {
	if (q->index2[q->hash & 255].v == q->px.v) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->hash & 255;
		return 1;
	}
	return 0;
}
void qoip_dec_index8(qoip_working_t *q) {
	++q->p;
	q->px = q->index2[q->in[q->p++]];
}

int qoip_enc_index9(qoip_working_t *q, u8 opcode) {
	if (q->index2[q->hash & 511].v == q->px.v) {
		q->out[q->p++] = opcode | ((q->hash & 511) >> 8);
		q->out[q->p++] = q->hash & 255;
		return 1;
	}
	return 0;
}
void qoip_dec_index9(qoip_working_t *q) {
	int index = (q->in[q->p++] & 1) << 8;
	index |= q->in[q->p++];
	q->px = q->index2[index];
}

int qoip_enc_index10(qoip_working_t *q, u8 opcode) {
	if (q->index2[q->hash & 1023].v == q->px.v) {
		q->out[q->p++] = opcode | ((q->hash & 1023) >> 8);
		q->out[q->p++] = q->hash & 255;
		return 1;
	}
	return 0;
}
void qoip_dec_index10(qoip_working_t *q) {
	int index = (q->in[q->p++] & 3) << 8;
	index |= q->in[q->p++];
	q->px = q->index2[index];
}

int qoip_enc_delta(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_r > -2 && q->avg_r < 2 &&
		q->avg_g > -2 && q->avg_g < 2 &&
		q->avg_b > -2 && q->avg_b < 2
	) {
		q->out[q->p++] = opcode | (((q->avg_b + 1) * 9) + ((q->avg_g + 1) * 3) + (q->avg_r + 1));
		return 1;
	}
	else if (
		q->vr == 0 && q->vg == 0 && q->vb == 0 &&
		q->va > -3 && q->va < 3
	) {
		q->out[q->p++] = opcode | (29 + q->va);
		return 1;
	}
	return 0;
}
void qoip_dec_delta(qoip_working_t *q) {
	int b1=q->in[q->p++]&31;
	if(b1<27) {
		q->px.rgba.r = q->px_ref.rgba.r + ((b1 % 3) - 1);
		b1/=3;
		q->px.rgba.g = q->px_ref.rgba.g + ((b1 % 3) - 1);
		b1/=3;
		q->px.rgba.b = q->px_ref.rgba.b + ((b1 % 3) - 1);
	}
	else
		q->px.rgba.a += (b1 - 29);
}


int qoip_enc_diff1_222(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_r > -3 && q->avg_r < 2 &&
		q->avg_g > -3 && q->avg_g < 2 &&
		q->avg_b > -3 && q->avg_b < 2
	) {
		q->out[q->p++] = opcode | (q->avg_r + 2) << 4 | (q->avg_g + 2) << 2 | (q->avg_b + 2);
		return 1;
	}
	return 0;
}
void qoip_dec_diff1_222(qoip_working_t *q) {
	q->px.rgba.r = q->px_ref.rgba.r + ((q->in[q->p] >> 4) & 0x03) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + ((q->in[q->p] >> 2) & 0x03) - 2;
	q->px.rgba.b = q->px_ref.rgba.b + ( q->in[q->p]       & 0x03) - 2;
	++q->p;
}

int qoip_enc_luma1_232_bias(qoip_working_t *q, u8 opcode) {
	if ( q->va == 0 &&
		q->avg_g > -5 && q->avg_g < 4 &&
		q->avg_gr > -3 && q->avg_gr < 3 &&
		q->avg_gb > -3 && q->avg_gb < 3 ) {
		if (      q->avg_g <  0 && q->avg_gr > -2 && q->avg_gb > -2 ) {
			q->out[q->p++] = opcode | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
			return 1;
		}
		else if ( q->avg_g > -1 && q->avg_gr <  2 && q->avg_gb <  2 ) {
			q->out[q->p++] = opcode | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
			return 1;
		}
	}
	return 0;
}
void qoip_dec_luma1_232_bias(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int vg = ((b1 >> 4) & 7) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	if (vg < 0) {
		q->px.rgba.r = q->px_ref.rgba.r + vg - 1 + ((b1 >> 2) & 3);
		q->px.rgba.b = q->px_ref.rgba.b + vg - 1 +  (b1 &  3);
	}
	else {
		q->px.rgba.r = q->px_ref.rgba.r + vg - 2 + ((b1 >> 2) & 3);
		q->px.rgba.b = q->px_ref.rgba.b + vg - 2 +  (b1 &  3);
	}
}

int qoip_enc_deltaa(qoip_working_t *q, u8 opcode) {
	if (
		(q->va == -1 || q->va == 1) &&
		q->avg_r > -2 && q->avg_r < 2 &&
		q->avg_g > -2 && q->avg_g < 2 &&
		q->avg_b > -2 && q->avg_b < 2
	) {
		q->out[q->p++] = opcode | (q->va==1?32:0) | (((q->avg_b+1)*9)+((q->avg_g+1)*3)+(q->avg_r+1));
		return 1;
	}
	else if (/*encode small changes in a, -5..4*/
		q->vr == 0 && q->vg == 0 && q->vb == 0 &&
		q->va > -6 && q->va < 5
	) {
		if(q->va>=0)
			q->out[q->p++] = opcode | 32 | (27 + q->va);
		else
			q->out[q->p++] = opcode |      (32 + q->va);
		return 1;
	}
	return 0;
}
void qoip_dec_deltaa(qoip_working_t *q) {
	int b1=q->in[q->p++];
	switch(b1&31){
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
			if(b1&32)
				q->px.rgba.a += ((b1&31)-27);
			else
				q->px.rgba.a += ((b1&31)-32);
			break;
		default:
			q->px.rgba.a += (b1 & 32) ? 1 : -1;
			b1 &= 31;
			q->px.rgba.r = q->px_ref.rgba.r + ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.g = q->px_ref.rgba.g + ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.b = q->px_ref.rgba.b + ((b1 % 3) - 1);
			break;
	}
}

int qoip_enc_a(qoip_working_t *q, u8 opcode) {
	if ( q->vr == 0 && q->vg == 0 && q->vb == 0 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->px.rgba.a;
		return 1;
	}
	return 0;
}
void qoip_dec_a(qoip_working_t *q) {
	++q->p;
	q->px.rgba.a = q->in[q->p++];
}

/* Generated LUMA functions */
static inline int qoip_enc_luma1_222(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -3 && q->avg_g  < 2 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 2) << 4) | ((q->avg_gr + 2) << 2) | ((q->avg_gb + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma1_222(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int vg = ((b1 >> 4) & 3) - 2;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b1 >> 2) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b1 >> 0) & 3) - 2;
}

static inline int qoip_enc_luma1_232(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -5 && q->avg_g  < 4 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 4) << 4) | ((q->avg_gr + 2) << 2) | ((q->avg_gb + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma1_232(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int vg = ((b1 >> 4) & 7) - 4;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b1 >> 2) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b1 >> 0) & 3) - 2;
}

static inline int qoip_enc_luma2_242(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = ((q->avg_g + 8) << 4) | ((q->avg_gr + 2) << 2) | ((q->avg_gb + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_242(qoip_working_t *q) {
	++q->p;
	int b2 = q->in[q->p++];
	int vg = ((b2 >> 4) & 15) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 2) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 3) - 2;
}

static inline int qoip_enc_luma2_333(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -5 && q->avg_g  < 4 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 4) >> 2);
		q->out[q->p++] = ((q->avg_g + 4) & 3) << 6 | ((q->avg_gr + 4) << 3) | ((q->avg_gb + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_333(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 1) << 2) | (b2 >> 6)) - 4;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 3) & 7) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 7) - 4;
}

static inline int qoip_enc_luma2_343(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) >> 2);
		q->out[q->p++] = ((q->avg_g + 8) & 3) << 6 | ((q->avg_gr + 4) << 3) | ((q->avg_gb + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_343(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 3) << 2) | (b2 >> 6)) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 3) & 7) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 7) - 4;
}

static inline int qoip_enc_luma2_353(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) >> 2);
		q->out[q->p++] = ((q->avg_g + 16) & 3) << 6 | ((q->avg_gr + 4) << 3) | ((q->avg_gb + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_353(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 7) << 2) | (b2 >> 6)) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 3) & 7) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 7) - 4;
}

static inline int qoip_enc_luma2_444(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) << 0);
		q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_444(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 0) & 15) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 15) - 8;
}

static inline int qoip_enc_luma2_454(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) << 0);
		q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_454(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 0) & 31) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 15) - 8;
}

static inline int qoip_enc_luma2_464(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) << 0);
		q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_464(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 0) & 63) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 15) - 8;
}

static inline int qoip_enc_luma2_555(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) << 2) | ((q->avg_gr + 16) >> 3);
		q->out[q->p++] = (((q->avg_gr + 16) & 7) << 5) | ((q->avg_gb + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_555(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 2) & 31) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b1 & 3) << 3) | (b2 >> 5)) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 31) - 16;
}

static inline int qoip_enc_luma3_565(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = ((q->avg_g + 32) << 2) | ((q->avg_gr + 16) >> 3);
		q->out[q->p++] = (((q->avg_gr + 16) & 7) << 5) | ((q->avg_gb + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_565(qoip_working_t *q) {
	++q->p;
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = ((b2 >> 2) & 63) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 3) << 3) | (b3 >> 5)) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 31) - 16;
}

static inline int qoip_enc_luma3_575(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) >> 6);
		q->out[q->p++] = ((q->avg_g + 64) & 63) << 2 | ((q->avg_gr + 16) >> 3);
		q->out[q->p++] = (((q->avg_gr + 16) & 7) << 5) | ((q->avg_gb + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_575(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 1) << 6) | (b2 >> 2)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 3) << 3) | (b3 >> 5)) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 31) - 16;
}

static inline int qoip_enc_luma3_666(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) >> 4);
		q->out[q->p++] = ((q->avg_g + 32) & 15) << 4 | ((q->avg_gr + 32) >> 2);
		q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_666(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 3) << 4) | (b2 >> 4)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 2) | (b3 >> 6)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 63) - 32;
}

static inline int qoip_enc_luma3_676(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) >> 4);
		q->out[q->p++] = ((q->avg_g + 64) & 15) << 4 | ((q->avg_gr + 32) >> 2);
		q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_676(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 7) << 4) | (b2 >> 4)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 2) | (b3 >> 6)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 63) - 32;
}

static inline int qoip_enc_luma3_686(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 4);
		q->out[q->p++] = ((q->avg_g + 128) & 15) << 4 | ((q->avg_gr + 32) >> 2);
		q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_686(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 15) << 4) | (b2 >> 4)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 2) | (b3 >> 6)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 63) - 32;
}

static inline int qoip_enc_luma3_777(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -65 && q->avg_gr < 64 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -65 && q->avg_gb < 64 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) >> 2);
		q->out[q->p++] = ((q->avg_g + 64) & 3) << 6 | ((q->avg_gr + 64) >> 1);
		q->out[q->p++] = (((q->avg_gr + 64) & 1) << 7) | ((q->avg_gb + 64) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_777(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 31) << 2) | (b2 >> 6)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 63) << 1) | (b3 >> 7)) - 64;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 127) - 64;
}

static inline int qoip_enc_luma3_787(qoip_working_t *q, u8 opcode) {
	if ( q->va==0 &&
		q->avg_gr > -65 && q->avg_gr < 64 &&
		q->avg_gb > -65 && q->avg_gb < 64 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 2);
		q->out[q->p++] = ((q->avg_g + 128) & 3) << 6 | ((q->avg_gr + 64) >> 1);
		q->out[q->p++] = (((q->avg_gr + 64) & 1) << 7) | ((q->avg_gb + 64) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_787(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 63) << 2) | (b2 >> 6)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 63) << 1) | (b3 >> 7)) - 64;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 127) - 64;
}

static inline int qoip_enc_luma2_2321(qoip_working_t *q, u8 opcode) {
	if ( q->va > -2 && q->va < 1 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -5 && q->avg_g  < 4 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = ((q->avg_g + 4) << 5) | ((q->avg_gr + 2) << 3) | ((q->avg_gb + 2) << 1) | ((q->va + 1) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_2321(qoip_working_t *q) {
	++q->p;
	int b2 = q->in[q->p++];
	int vg = ((b2 >> 5) & 7) - 4;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 3) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 1) & 3) - 2;
	q->px.rgba.a += ((b2 & 1) - 1);
}

static inline int qoip_enc_luma2_2322(qoip_working_t *q, u8 opcode) {
	if ( q->va > -3 && q->va < 2 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -5 && q->avg_g  < 4 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 4) >> 2);
		q->out[q->p++] = ((q->avg_g + 4) & 3) << 6 | ((q->avg_gr + 2) << 4) | ((q->avg_gb + 2) << 2) | ((q->va + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_2322(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 1) << 2) | (b2 >> 6)) - 4;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 2) & 3) - 2;
	q->px.rgba.a += ((b2 & 3) - 2);
}

static inline int qoip_enc_luma2_2422(qoip_working_t *q, u8 opcode) {
	if ( q->va > -3 && q->va < 2 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) >> 2);
		q->out[q->p++] = ((q->avg_g + 8) & 3) << 6 | ((q->avg_gr + 2) << 4) | ((q->avg_gb + 2) << 2) | ((q->va + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_2422(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 3) << 2) | (b2 >> 6)) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 2) & 3) - 2;
	q->px.rgba.a += ((b2 & 3) - 2);
}

static inline int qoip_enc_luma2_2423(qoip_working_t *q, u8 opcode) {
	if ( q->va > -5 && q->va < 4 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) >> 1);
		q->out[q->p++] = ((q->avg_g + 8) & 1) << 7 | ((q->avg_gr + 2) << 5) | ((q->avg_gb + 2) << 3) | ((q->va + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_2423(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (((b1 & 7) << 1) | (b2 >> 7)) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 5) & 3) - 2;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 3) & 3) - 2;
	q->px.rgba.a += ((b2 & 7) - 4);
}

static inline int qoip_enc_luma2_3432(qoip_working_t *q, u8 opcode) {
	if ( q->va > -3 && q->va < 2 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) << 0);
		q->out[q->p++] = ((q->avg_gr + 4) << 5) | ((q->avg_gb + 4) << 2) | ((q->va + 2) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_3432(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 0) & 15) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 5) & 7) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 2) & 7) - 4;
	q->px.rgba.a += ((b2 & 3) - 2);
}

static inline int qoip_enc_luma2_3433(qoip_working_t *q, u8 opcode) {
	if ( q->va > -5 && q->va < 4 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -9 && q->avg_g  < 8 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 8) << 1) | ((q->avg_gr + 4) >> 2);
		q->out[q->p++] = (((q->avg_gr + 4) & 3) << 6) | ((q->avg_gb + 4) << 3) | ((q->va + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_3433(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 1) & 15) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b1 & 1) << 2) | (b2 >> 6)) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 3) & 7) - 4;
	q->px.rgba.a += ((b2 & 7) - 4);
}

static inline int qoip_enc_luma2_3533(qoip_working_t *q, u8 opcode) {
	if ( q->va > -5 && q->va < 4 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) << 1) | ((q->avg_gr + 4) >> 2);
		q->out[q->p++] = (((q->avg_gr + 4) & 3) << 6) | ((q->avg_gb + 4) << 3) | ((q->va + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_3533(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 1) & 31) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b1 & 1) << 2) | (b2 >> 6)) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 3) & 7) - 4;
	q->px.rgba.a += ((b2 & 7) - 4);
}

static inline int qoip_enc_luma2_3534(qoip_working_t *q, u8 opcode) {
	if ( q->va > -9 && q->va < 8 &&
		q->avg_gr > -5 && q->avg_gr < 4 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -5 && q->avg_gb < 4 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) << 2) | ((q->avg_gr + 4) >> 1);
		q->out[q->p++] = (((q->avg_gr + 4) & 1) << 7) | ((q->avg_gb + 4) << 4) | ((q->va + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma2_3534(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = ((b1 >> 2) & 31) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b1 & 3) << 1) | (b2 >> 7)) - 4;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 4) & 7) - 4;
	q->px.rgba.a += ((b2 & 15) - 8);
}

static inline int qoip_enc_luma3_4543(qoip_working_t *q, u8 opcode) {
	if ( q->va > -5 && q->va < 4 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = ((q->avg_g + 16) << 3) | ((q->avg_gr + 8) >> 1);
		q->out[q->p++] = (((q->avg_gr + 8) & 1) << 7) | ((q->avg_gb + 8) << 3) | ((q->va + 4) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_4543(qoip_working_t *q) {
	++q->p;
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = ((b2 >> 3) & 31) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 7) << 1) | (b3 >> 7)) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 3) & 15) - 8;
	q->px.rgba.a += ((b3 & 7) - 4);
}

static inline int qoip_enc_luma3_4544(qoip_working_t *q, u8 opcode) {
	if ( q->va > -9 && q->va < 8 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -17 && q->avg_g  < 16 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 16) >> 4);
		q->out[q->p++] = ((q->avg_g + 16) & 15) << 4 | ((q->avg_gr + 8) << 0);
		q->out[q->p++] = ((q->avg_gb + 8) << 4) | ((q->va + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_4544(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 1) << 4) | (b2 >> 4)) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 0) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 4) & 15) - 8;
	q->px.rgba.a += ((b3 & 15) - 8);
}

static inline int qoip_enc_luma3_4644(qoip_working_t *q, u8 opcode) {
	if ( q->va > -9 && q->va < 8 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) >> 4);
		q->out[q->p++] = ((q->avg_g + 32) & 15) << 4 | ((q->avg_gr + 8) << 0);
		q->out[q->p++] = ((q->avg_gb + 8) << 4) | ((q->va + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_4644(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 3) << 4) | (b2 >> 4)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 0) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 4) & 15) - 8;
	q->px.rgba.a += ((b3 & 15) - 8);
}

static inline int qoip_enc_luma3_4645(qoip_working_t *q, u8 opcode) {
	if ( q->va > -17 && q->va < 16 &&
		q->avg_gr > -9 && q->avg_gr < 8 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -9 && q->avg_gb < 8 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) >> 3);
		q->out[q->p++] = ((q->avg_g + 32) & 7) << 5 | ((q->avg_gr + 8) << 1) | ((q->avg_gb + 8) >> 3);
		q->out[q->p++] = (((q->avg_gb + 8) & 7) << 5) | ((q->va + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_4645(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 7) << 3) | (b2 >> 5)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 1) & 15) - 8;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 1) << 3) | (b3 >> 5)) - 8;
	q->px.rgba.a += ((b3 & 31) - 16);
}

static inline int qoip_enc_luma3_5654(qoip_working_t *q, u8 opcode) {
	if ( q->va > -9 && q->va < 8 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) >> 2);
		q->out[q->p++] = ((q->avg_g + 32) & 3) << 6 | ((q->avg_gr + 16) << 1) | ((q->avg_gb + 16) >> 4);
		q->out[q->p++] = (((q->avg_gb + 16) & 15) << 4) | ((q->va + 8) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_5654(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 15) << 2) | (b2 >> 6)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 1) & 31) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 1) << 4) | (b3 >> 4)) - 16;
	q->px.rgba.a += ((b3 & 15) - 8);
}

static inline int qoip_enc_luma3_5655(qoip_working_t *q, u8 opcode) {
	if ( q->va > -17 && q->va < 16 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -33 && q->avg_g  < 32 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 32) >> 1);
		q->out[q->p++] = ((q->avg_g + 32) & 1) << 7 | ((q->avg_gr + 16) << 2) | ((q->avg_gb + 16) >> 3);
		q->out[q->p++] = (((q->avg_gb + 16) & 7) << 5) | ((q->va + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_5655(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 31) << 1) | (b2 >> 7)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 2) & 31) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 3) << 3) | (b3 >> 5)) - 16;
	q->px.rgba.a += ((b3 & 31) - 16);
}

static inline int qoip_enc_luma3_5755(qoip_working_t *q, u8 opcode) {
	if ( q->va > -17 && q->va < 16 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) >> 1);
		q->out[q->p++] = ((q->avg_g + 64) & 1) << 7 | ((q->avg_gr + 16) << 2) | ((q->avg_gb + 16) >> 3);
		q->out[q->p++] = (((q->avg_gb + 16) & 7) << 5) | ((q->va + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_5755(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 63) << 1) | (b2 >> 7)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 2) & 31) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 3) << 3) | (b3 >> 5)) - 16;
	q->px.rgba.a += ((b3 & 31) - 16);
}

static inline int qoip_enc_luma3_5756(qoip_working_t *q, u8 opcode) {
	if ( q->va > -33 && q->va < 32 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -17 && q->avg_gb < 16 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) << 0);
		q->out[q->p++] = ((q->avg_gr + 16) << 3) | ((q->avg_gb + 16) >> 2);
		q->out[q->p++] = (((q->avg_gb + 16) & 3) << 6) | ((q->va + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma3_5756(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = ((b1 >> 0) & 127) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 3) & 31) - 16;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 7) << 2) | (b3 >> 6)) - 16;
	q->px.rgba.a += ((b3 & 63) - 32);
}

static inline int qoip_enc_luma4_6765(qoip_working_t *q, u8 opcode) {
	if ( q->va > -17 && q->va < 16 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = ((q->avg_g + 64) << 1) | ((q->avg_gr + 32) >> 5);
		q->out[q->p++] = (((q->avg_gr + 32) & 31) << 3) | ((q->avg_gb + 32) >> 3);
		q->out[q->p++] = (((q->avg_gb + 32) & 7) << 5) | ((q->va + 16) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_6765(qoip_working_t *q) {
	++q->p;
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = ((b2 >> 1) & 127) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 1) << 5) | (b3 >> 3)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 7) << 3) | (b4 >> 5)) - 32;
	q->px.rgba.a += ((b4 & 31) - 16);
}

static inline int qoip_enc_luma4_6766(qoip_working_t *q, u8 opcode) {
	if ( q->va > -33 && q->va < 32 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_g  > -65 && q->avg_g  < 64 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 64) >> 6);
		q->out[q->p++] = ((q->avg_g + 64) & 63) << 2 | ((q->avg_gr + 32) >> 4);
		q->out[q->p++] = (((q->avg_gr + 32) & 15) << 4) | ((q->avg_gb + 32) >> 2);
		q->out[q->p++] = (((q->avg_gb + 32) & 3) << 6) | ((q->va + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_6766(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b1 & 1) << 6) | (b2 >> 2)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 3) << 4) | (b3 >> 4)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 15) << 2) | (b4 >> 6)) - 32;
	q->px.rgba.a += ((b4 & 63) - 32);
}

static inline int qoip_enc_luma4_6866(qoip_working_t *q, u8 opcode) {
	if ( q->va > -33 && q->va < 32 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 6);
		q->out[q->p++] = ((q->avg_g + 128) & 63) << 2 | ((q->avg_gr + 32) >> 4);
		q->out[q->p++] = (((q->avg_gr + 32) & 15) << 4) | ((q->avg_gb + 32) >> 2);
		q->out[q->p++] = (((q->avg_gb + 32) & 3) << 6) | ((q->va + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_6866(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b1 & 3) << 6) | (b2 >> 2)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 3) << 4) | (b3 >> 4)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 15) << 2) | (b4 >> 6)) - 32;
	q->px.rgba.a += ((b4 & 63) - 32);
}

static inline int qoip_enc_luma4_6867(qoip_working_t *q, u8 opcode) {
	if ( q->va > -65 && q->va < 64 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_gb > -33 && q->avg_gb < 32 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 5);
		q->out[q->p++] = ((q->avg_g + 128) & 31) << 3 | ((q->avg_gr + 32) >> 3);
		q->out[q->p++] = (((q->avg_gr + 32) & 7) << 5) | ((q->avg_gb + 32) >> 1);
		q->out[q->p++] = (((q->avg_gb + 32) & 1) << 7) | ((q->va + 64) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_6867(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b1 & 7) << 5) | (b2 >> 3)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 7) << 3) | (b3 >> 5)) - 32;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 31) << 1) | (b4 >> 7)) - 32;
	q->px.rgba.a += ((b4 & 127) - 64);
}

static inline int qoip_enc_luma4_7876(qoip_working_t *q, u8 opcode) {
	if ( q->va > -33 && q->va < 32 &&
		q->avg_gr > -65 && q->avg_gr < 64 &&
		q->avg_gb > -65 && q->avg_gb < 64 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 4);
		q->out[q->p++] = ((q->avg_g + 128) & 15) << 4 | ((q->avg_gr + 64) >> 3);
		q->out[q->p++] = (((q->avg_gr + 64) & 7) << 5) | ((q->avg_gb + 64) >> 2);
		q->out[q->p++] = (((q->avg_gb + 64) & 3) << 6) | ((q->va + 32) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_7876(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b1 & 15) << 4) | (b2 >> 4)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 3) | (b3 >> 5)) - 64;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 31) << 2) | (b4 >> 6)) - 64;
	q->px.rgba.a += ((b4 & 63) - 32);
}

static inline int qoip_enc_luma4_7877(qoip_working_t *q, u8 opcode) {
	if ( q->va > -65 && q->va < 64 &&
		q->avg_gr > -65 && q->avg_gr < 64 &&
		q->avg_gb > -65 && q->avg_gb < 64 ) {
		q->out[q->p++] = opcode | ((q->avg_g + 128) >> 3);
		q->out[q->p++] = ((q->avg_g + 128) & 7) << 5 | ((q->avg_gr + 64) >> 2);
		q->out[q->p++] = (((q->avg_gr + 64) & 3) << 6) | ((q->avg_gb + 64) >> 1);
		q->out[q->p++] = (((q->avg_gb + 64) & 1) << 7) | ((q->va + 64) << 0);
		return 1;
	}
	return 0;
}
static inline void qoip_dec_luma4_7877(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b1 & 31) << 3) | (b2 >> 5)) - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 31) << 2) | (b3 >> 6)) - 64;
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 63) << 1) | (b4 >> 7)) - 64;
	q->px.rgba.a += ((b4 & 127) - 64);
}

