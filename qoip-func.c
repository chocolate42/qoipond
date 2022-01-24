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

int qoip_enc_index8(qoip_working_t *q, u8 opcode) {
	if (q->index2[q->hash].v == q->px.v) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->hash;
		return 1;
	}
	return 0;
}
void qoip_dec_index8(qoip_working_t *q) {
	++q->p;
	q->px = q->index2[q->in[q->p++]];
}

/* === Length 1 RGB delta functions */
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
	if (
		q->va == 0 &&
		q->avg_g   > -5 && q->avg_g   < 0 &&
		q->avg_gr > -2 && q->avg_gr < 3 &&
		q->avg_gb > -2 && q->avg_gb < 3
	) {
		q->out[q->p++] = opcode | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
		return 1;
	}
	else if (
		q->va == 0 &&
		q->avg_g   > -1 && q->avg_g   < 4 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_gb > -3 && q->avg_gb < 2
	) {
		q->out[q->p++] = opcode | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
		return 1;
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

int qoip_enc_luma1_232(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g >   -5 && q->avg_g <   4 &&
		q->avg_gb > -3 && q->avg_gb < 2
	) {
		q->out[q->p++] = opcode | ((q->avg_g + 4) << 4) | ((q->avg_gr + 2) << 2) | (q->avg_gb + 2);
		return 1;
	}
	return 0;
}
void qoip_dec_luma1_232(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int vg = ((b1 >> 4) - 4);
	q->px.rgba.r = q->px_ref.rgba.r + vg - 2 + ((b1 >> 2) & 0x03);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 2 + ((b1     ) & 0x03);
}

/* === Length 2 RGB delta functions */
int qoip_enc_luma2_454(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr >  -9 && q->avg_gr <  8 &&
		q->avg_g   > -17 && q->avg_g   < 16 &&
		q->avg_gb >  -9 && q->avg_gb <  8
	) {
		q->out[q->p++] = opcode               | (q->avg_g  + 16);
		q->out[q->p++] = (q->avg_gr + 8) << 4 | (q->avg_gb +  8);
		return 1;
	}
	return 0;
}
void qoip_dec_luma2_454(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b1 & 0x1f) - 16;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 8 + ((b2 >> 4) & 0x0f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 8 +  (b2       & 0x0f);
}

int qoip_enc_luma2_464(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr >  -9 && q->avg_gr <  8 &&
		q->avg_g   > -33 && q->avg_g   < 32 &&
		q->avg_gb >  -9 && q->avg_gb <  8
	) {
		q->out[q->p++] = opcode             | (q->avg_g   + 32);
		q->out[q->p++] = (q->avg_gr + 8) << 4 | (q->avg_gb +  8);
		return 1;
	}
	return 0;
}
void qoip_dec_luma2_464(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b1 & 0x3f) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 8 + ((b2 >> 4) & 0x0f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 8 +  (b2       & 0x0f);
}

/* === Length 3 RGB delta functions */
int qoip_enc_luma3_676(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr > -33 && q->avg_gr < 32 &&
		q->avg_g   > -65 && q->avg_g   < 64 &&
		q->avg_gb > -33 && q->avg_gb < 32
	) {
		q->out[q->p++] = opcode  | ((q->avg_g + 64) >> 4);
		q->out[q->p++] = (((q->avg_g   + 64) & 0x0f) << 4) | ((q->avg_gb + 32) >> 2);
		q->out[q->p++] = (((q->avg_gb + 32) & 0x03) << 6) | ( q->avg_gr + 32      );
		return 1;
	}
	return 0;
}
void qoip_dec_luma3_676(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 0x07) << 4) | (b2 >> 4)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 32 + (b3 & 0x3f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 32 + (((b2 & 0x0f) << 2) | ((b3 >> 6) & 0x03));
}


int qoip_enc_luma3_686(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr >  -33 && q->avg_gr <  32 &&
		q->avg_gb >  -33 && q->avg_gb <  32
	) {
		q->out[q->p++] = opcode                      | ((q->avg_gr + 32) >> 2);
		q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) |  (q->avg_gb + 32);
		q->out[q->p++] = q->avg_g + 128;
		return 1;
	}
	return 0;
}
void qoip_dec_luma3_686(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = b3 - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 32 + (((b1 & 0x0f) << 2) | ((b2) >> 6));
	q->px.rgba.b = q->px_ref.rgba.b + vg - 32 + (b2 & 0x3f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
}


int qoip_enc_luma3_787(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->avg_gr >  -65 && q->avg_gr <  64 &&
		q->avg_gb >  -65 && q->avg_gb <  64
	) {
		q->out[q->p++] = opcode                      | ((q->avg_gr + 64) >> 1);
		q->out[q->p++] = q->avg_g + 128;
		q->out[q->p++] = (((q->avg_gr + 64) & 1) << 7) |  (q->avg_gb + 64);
		return 1;
	}
	return 0;
}
void qoip_dec_luma3_787(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = b2 - 128;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 64 + (((b1 & 0x3f) << 1) | ((b3) >> 7));
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 64 + (b3 & 0x7f);
}

/* === length 1 RGBA delta functions */
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

/* === Length 2 RGBA delta functions */
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

int qoip_enc_luma2_3433(qoip_working_t *q, u8 opcode) {
	if (
		q->va   >  -5 && q->va   <  4 &&
		q->avg_gr >  -5 && q->avg_gr <  4 &&
		q->avg_g   >  -9 && q->avg_g   <  8 &&
		q->avg_gb >  -5 && q->avg_gb <  4
	) {/* tttrrrbb baaagggg */
		q->out[q->p++] = opcode      | (q->avg_gr + 4) << 2 | (q->avg_gb + 4) >> 1;
		q->out[q->p++] = ((q->avg_gb + 4) & 1) << 7 | (q->va + 4) << 4 | (q->avg_g + 8);
		return 1;
	}
	return 0;
}
void qoip_dec_luma2_3433(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b2 & 0xf) - 8;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 4 + ((b1 >> 2) & 7);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 4 + ((b1 & 3) << 1) + (b2 >> 7);
	q->px.rgba.a += ((b2 >> 4) & 7) - 4;
}

/* === Length 3 RGBA delta functions */
int qoip_enc_luma3_4645(qoip_working_t *q, u8 opcode) {
	if (
		q->va   > -17 && q->va   < 16 &&
		q->avg_gr >  -9 && q->avg_gr <  8 &&
		q->avg_g   > -33 && q->avg_g   < 32 &&
		q->avg_gb >  -9 && q->avg_gb <  8
	) {
		q->out[q->p++] = opcode      | ((q->avg_g   + 32) >> 3);
		q->out[q->p++] = (((q->avg_g + 32) & 0x07) << 5) | (q->va +  16);
		q->out[q->p++] = (q->avg_gr + 8) << 4 | (q->avg_gb +  8);
		return 1;
	}
	return 0;
}
void qoip_dec_luma3_4645(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (((b1 & 0x07) << 3) | (b2 >> 5)) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 8 + ((b3 >> 4) & 0x0f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 8 +  (b3       & 0x0f);
	q->px.rgba.a += (b2 & 0x1f) - 16;
}

int qoip_enc_luma3_5654(qoip_working_t *q, u8 opcode) {
	if (
		q->va   >  -9 && q->va    < 8 &&
		q->avg_gr > -17 && q->avg_gr < 16 &&
		q->avg_g   > -33 && q->avg_g   < 32 &&
		q->avg_gb > -17 && q->avg_gb < 16
	) {/* ttttaaaa rrrrrbbb bbgggggg */
		q->out[q->p++] = opcode | (q->va   + 8);
		q->out[q->p++] = (q->avg_gr + 16) << 3 | ((q->avg_gb + 16) >> 2);
		q->out[q->p++] = ((q->avg_gb + 16) & 3) << 6 | (q->avg_g + 32);
		return 1;
	}
	return 0;
}
void qoip_dec_luma3_5654(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (b3 & 0x3f) - 32;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 16 + ((b2 >> 3) & 0x1f);
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 16 +  (((b2 & 7) << 2) | (b3 >> 6));
	q->px.rgba.a += (b1 & 0x0f) - 8;
}

/* === Length 4 RGBA delta functions */
int qoip_enc_luma4_7777(qoip_working_t *q, u8 opcode) {
	if (
		q->va   > -65 && q->va   < 64 &&
		q->avg_gr > -65 && q->avg_gr < 64 &&
		q->avg_g   > -65 && q->avg_g   < 64 &&
		q->avg_gb > -65 && q->avg_gb < 64
	) {/* ttttrrrr rrrggggg ggbbbbbb baaaaaaa */
		q->out[q->p++] = opcode                      | ((q->avg_gr + 64) >> 3);
		q->out[q->p++] = (((q->avg_gr + 64) & 7) << 5) | ((q->avg_g   + 64) >> 2);
		q->out[q->p++] = (((q->avg_g   + 64) & 3) << 6) | ((q->avg_gb + 64) >> 1);
		q->out[q->p++] = (((q->avg_gb + 64) & 1) << 7) | ((q->va   + 64)     );
		return 1;
	}
	return 0;
}
void qoip_dec_luma4_7777(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b2 & 0x1f) << 2) | (b3 >> 6)) - 64;
	q->px.rgba.r = q->px_ref.rgba.r + vg - 64 + (((b1 & 0x0f) << 3) | (b2 >> 5));
	q->px.rgba.g = q->px_ref.rgba.g + vg;
	q->px.rgba.b = q->px_ref.rgba.b + vg - 64 + (((b3 & 0x3f) << 1) | (b4 >> 7));
	q->px.rgba.a += (b4 & 0x7f) - 64;
}
