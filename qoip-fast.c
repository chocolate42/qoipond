/* SPDX-License-Identifier: MIT */
/* Fastpath implementations used by qoip.h implementation. Included by QOIP_C only

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

* Encode and decode functions start processing pixels immediately, header and
  state handling has been dealt with
* Footer needs to be written by the encode functions
*/

enum{QOIP_MASK_1=0x80, QOIP_MASK_2=0xc0, QOIP_MASK_3=0xe0, QOIP_MASK_4=0xf0, QOIP_MASK_5=0xf8, QOIP_MASK_6=0xfc, QOIP_MASK_7=0xfe};

/* -effort 0 */
enum{E0_LUMA1_232B=0x00, E0_LUMA2_464=0x80, E0_INDEX5=0xc0, E0_LUMA3_676=0xe0, E0_INDEX10=0xe8, E0_LUMA4_6866=0xec, E0_LUMA2_2322=0xf0, E0_LUMA3_4544=0xf2, E0_A=0xf4, E0_RGB=0xf5, E0_RGBA=0xf6, E0_RUN2=0xf7, E0_RUN1=0xf8};

int qoip_encode_effort0(qoip_working_t *q, size_t *out_len, void *scratch, int entropy) {
	int index_pos;
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px = *(qoip_rgba_t *)(q->in + q->px_pos);

				/*Run*/
				if (q->px.v == q->px_prev.v) {
					++q->run;
					goto eop4;
				}
				qoip_encode_run(q);

				/*Index1*/
				q->hash = QOIP_COLOR_HASH(q->px);
				index_pos = q->hash & 31;
				if (q->index[index_pos].v == q->px.v) {
					q->out[q->p++] = E0_INDEX5 | index_pos;
					goto eop4;
				}
				q->index[index_pos] = q->px;

				qoip_gen_var_rgb(q);
				q->va = q->px.rgba.a - q->px_prev.rgba.a;

				/*232B*/
				if ( q->va == 0 && q->avg_g > -5 && q->avg_g < 4 &&
						q->avg_gr > -3 && q->avg_gr < 3 &&
						q->avg_gb > -3 && q->avg_gb < 3 ) {
					if (      q->avg_g <  0 && q->avg_gr > -2 && q->avg_gb > -2 ) {
						q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
						goto eop4;
					}
					else if ( q->avg_g > -1 && q->avg_gr <  2 && q->avg_gb <  2 ) {
						q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
						goto eop4;
					}
				}

				/*Index2*/
				if (q->index2[q->hash & 1023].v == q->px.v) {
					q->out[q->p++] = E0_INDEX10 | ((q->hash & 1023) >> 8);
					q->out[q->p++] = q->hash & 255;
					goto eop4;
				}

				/*luma/fallback*/
				if(q->va!=0) {
					if ( q->vr == 0 && q->vg == 0 && q->vb == 0 ) {
						q->out[q->p++] = E0_A;
						q->out[q->p++] = q->px.rgba.a;
					}
					else if ( q->va > -3 && q->va < 2 &&
						q->avg_gr > -3 && q->avg_gr < 2 &&
						q->avg_g  > -5 && q->avg_g  < 4 &&
						q->avg_gb > -3 && q->avg_gb < 2 ) {
						q->out[q->p++] = E0_LUMA2_2322 | ((q->avg_g + 4) >> 2);
						q->out[q->p++] = ((q->avg_g + 4) & 3) << 6 | ((q->avg_gr + 2) << 4) | ((q->avg_gb + 2) << 2) | ((q->va + 2) << 0);
					}
					else if ( q->va > -9 && q->va < 8 &&
						q->avg_gr > -9 && q->avg_gr < 8 &&
						q->avg_g  > -17 && q->avg_g  < 16 &&
						q->avg_gb > -9 && q->avg_gb < 8 ) {
						q->out[q->p++] = E0_LUMA3_4544 | ((q->avg_g + 16) >> 4);
						q->out[q->p++] = ((q->avg_g + 16) & 15) << 4 | ((q->avg_gr + 8) << 0);
						q->out[q->p++] = ((q->avg_gb + 8) << 4) | ((q->va + 8) << 0);
					}
					else if ( q->va > -33 && q->va < 32 &&
						q->avg_gr > -33 && q->avg_gr < 32 &&
						q->avg_gb > -33 && q->avg_gb < 32 ) {
						q->out[q->p++] = E0_LUMA4_6866 | ((q->avg_g + 128) >> 6);
						q->out[q->p++] = ((q->avg_g + 128) & 63) << 2 | ((q->avg_gr + 32) >> 4);
						q->out[q->p++] = (((q->avg_gr + 32) & 15) << 4) | ((q->avg_gb + 32) >> 2);
						q->out[q->p++] = (((q->avg_gb + 32) & 3) << 6) | ((q->va + 32) << 0);
					}
					else {
						q->out[q->p++] = E0_RGBA;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
						q->out[q->p++] = q->px.rgba.a;
					}
				}
				else {
					if ( q->avg_gr > -9 && q->avg_gr < 8 &&
						q->avg_g  > -33 && q->avg_g  < 32 &&
						q->avg_gb > -9 && q->avg_gb < 8 ) {
						q->out[q->p++] = E0_LUMA2_464 | ((q->avg_g + 32) << 0);
						q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
					}
					else if ( q->avg_gr > -33 && q->avg_gr < 32 &&
						q->avg_g  > -65 && q->avg_g  < 64 &&
						q->avg_gb > -33 && q->avg_gb < 32 ) {
						q->out[q->p++] = E0_LUMA3_676 | ((q->avg_g + 64) >> 4);
						q->out[q->p++] = ((q->avg_g + 64) & 15) << 4 | ((q->avg_gr + 32) >> 2);
						q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
					}
					else {
						q->out[q->p++] = E0_RGB;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
					}
				}

				eop4:
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
				q->index2[q->hash & 1023] = q->px;
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

				/*Run*/
				if (q->px.v == q->px_prev.v) {
					++q->run;
					goto eop3;
				}
				qoip_encode_run(q);

				/*Index1*/
				q->hash = QOIP_COLOR_HASH(q->px);
				index_pos = q->hash & 31;
				if (q->index[index_pos].v == q->px.v) {
					q->out[q->p++] = E0_INDEX5 | index_pos;
					goto eop3;
				}
				q->index[index_pos] = q->px;

				qoip_gen_var_rgb(q);

				/*232B*/
				if ( q->va == 0 && q->avg_g > -5 && q->avg_g < 4 &&
						q->avg_gr > -3 && q->avg_gr < 3 &&
						q->avg_gb > -3 && q->avg_gb < 3 ) {
					if (      q->avg_g <  0 && q->avg_gr > -2 && q->avg_gb > -2 ) {
						q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
						goto eop3;
					}
					else if ( q->avg_g > -1 && q->avg_gr <  2 && q->avg_gb <  2 ) {
						q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
						goto eop3;
					}
				}

				/*Index2*/
				if (q->index2[q->hash & 1023].v == q->px.v) {
					q->out[q->p++] = E0_INDEX10 | ((q->hash & 1023) >> 8);
					q->out[q->p++] = q->hash & 255;
					goto eop3;
				}

				/*luma/fallback*/
				{
					if ( q->avg_gr > -9 && q->avg_gr < 8 &&
						q->avg_g  > -33 && q->avg_g  < 32 &&
						q->avg_gb > -9 && q->avg_gb < 8 ) {
						q->out[q->p++] = E0_LUMA2_464 | ((q->avg_g + 32) << 0);
						q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
					}
					else if ( q->avg_gr > -33 && q->avg_gr < 32 &&
						q->avg_g  > -65 && q->avg_g  < 64 &&
						q->avg_gb > -33 && q->avg_gb < 32 ) {
						q->out[q->p++] = E0_LUMA3_676 | ((q->avg_g + 64) >> 4);
						q->out[q->p++] = ((q->avg_g + 64) & 15) << 4 | ((q->avg_gr + 32) >> 2);
						q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
					}
					else {
						q->out[q->p++] = E0_RGB;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
					}
				}

				eop3:
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
				q->index2[q->hash & 1023] = q->px;
				q->px_pos +=3;
			}
		}
	}
	qoip_encode_run(q);
	qoip_finish(q);
	*out_len=q->p;

	if(entropy)
		qoip_entropy(q->out, out_len, scratch, entropy);
	return 0;
}

static inline void qoip_decode_effort0_inner(qoip_working_t *q) {
	int b1, b2, b3, b4, index, vg;
	if (q->run > 0)
		--q->run;
	else if (q->p < q->in_tot) {
		q->px_prev.v = q->px.v;
		if (q->px_pos >= q->stride && q->px_w<8192) {
			q->px_ref.rgba.r = (q->px.rgba.r + q->upcache[(q->px_w * 3) + 0] + 1) >> 1;
			q->px_ref.rgba.g = (q->px.rgba.g + q->upcache[(q->px_w * 3) + 1] + 1) >> 1;
			q->px_ref.rgba.b = (q->px.rgba.b + q->upcache[(q->px_w * 3) + 2] + 1) >> 1;
		}
		else
			q->px_ref.v = q->px_prev.v;

		b1 = q->in[q->p++];
		if(      (b1 & QOIP_MASK_1) == E0_LUMA1_232B ) {
			vg = ((b1 >> 4) & 7) - 4;
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
		else if( (b1 & QOIP_MASK_2) == E0_LUMA2_464 ) {
			b2 = q->in[q->p++];
			vg = ((b1 >> 0) & 63) - 32;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 15) - 8;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 15) - 8;
		}
		else if( (b1 & QOIP_MASK_3) == E0_INDEX5 ) {
			q->px = q->index[b1 & 31];
		}
		else if( (b1 & QOIP_MASK_5) == E0_LUMA3_676 ) {
			b2 = q->in[q->p++];
			b3 = q->in[q->p++];
			vg = (((b1 & 7) << 4) | (b2 >> 4)) - 64;
			q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 2) | (b3 >> 6)) - 32;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 63) - 32;
		}
		else if( (b1 & QOIP_MASK_6) == E0_INDEX10 ) {
			index = (b1 & 3) << 8;
			index |= q->in[q->p++];
			q->px = q->index2[index];
		}
		else if( (b1 & QOIP_MASK_6) == E0_LUMA4_6866 ) {
			b2 = q->in[q->p++];
			b3 = q->in[q->p++];
			b4 = q->in[q->p++];
			vg = (((b1 & 3) << 6) | (b2 >> 2)) - 128;
			q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 3) << 4) | (b3 >> 4)) - 32;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + (((b3 & 15) << 2) | (b4 >> 6)) - 32;
			q->px.rgba.a += ((b4 & 63) - 32);
		}
		else if( (b1 & QOIP_MASK_7) == E0_LUMA2_2322 ) {
			b2 = q->in[q->p++];
			vg = (((b1 & 1) << 2) | (b2 >> 6)) - 4;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 3) - 2;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 2) & 3) - 2;
			q->px.rgba.a += ((b2 & 3) - 2);
		}
		else if( (b1 & QOIP_MASK_7) == E0_LUMA3_4544 ) {
			b2 = q->in[q->p++];
			b3 = q->in[q->p++];
			vg = (((b1 & 1) << 4) | (b2 >> 4)) - 16;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 0) & 15) - 8;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 4) & 15) - 8;
			q->px.rgba.a += ((b3 & 15) - 8);
		}
		else if( b1 == E0_A ) {
			q->px.rgba.a = q->in[q->p++];
		}
		else if( b1 == E0_RGB ) {
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
		}
		else if( b1 == E0_RGBA ) {
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
			q->px.rgba.a = q->in[q->p++];
		}
		else if( b1 == E0_RUN2 )
			q->run = q->in[q->p++] + 8;
		else
			q->run = b1 - E0_RUN1;
	}
	q->index[QOIP_COLOR_HASH(q->px)  & q->index1_maxval] = q->px;
	q->index2[QOIP_COLOR_HASH(q->px) & q->index2_maxval] = q->px;
	if(q->px_w<8192) {
		q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
		q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
		q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
	}
}

int qoip_decode_effort0(qoip_working_t *q) {
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_effort0_inner(q);
				*(qoip_rgba_t*)(q->out + q->px_pos) = q->px;
				q->px_pos += 4;
			}
		}
	}
	else {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_effort0_inner(q);
				q->out[q->px_pos + 0] = q->px.rgba.r;
				q->out[q->px_pos + 1] = q->px.rgba.g;
				q->out[q->px_pos + 2] = q->px.rgba.b;
				q->px_pos += 3;
			}
		}
	}
	return 0;
}

/* -effort -1 */
enum{FAST1_LUMA1_232=0x00, FAST1_LUMA2_454=0x80, FAST1_LUMA2_3433=0xa0, FAST1_LUMA3_5655=0xc0, FAST1_LUMA3_676=0xe0, FAST1_RGB=0xe8, FAST1_RGBA=0xe9, FAST1_RUN2=0xea, FAST1_RUN1=0xeb};

int qoip_encode_fast1(qoip_working_t *q, size_t *out_len, void *scratch, int entropy) {
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px = *(qoip_rgba_t *)(q->in + q->px_pos);

				if (q->px.v == q->px_prev.v) {
					++q->run;
					goto eop4;
				}
				qoip_encode_run(q);
				qoip_gen_var_rgb(q);
				q->va = q->px.rgba.a - q->px_prev.rgba.a;

				if(q->va!=0) {
					if ( q->va > -5 && q->va < 4 &&
						q->avg_gr > -5 && q->avg_gr < 4 &&
						q->avg_g  > -9 && q->avg_g  < 8 &&
						q->avg_gb > -5 && q->avg_gb < 4 ) {
						q->out[q->p++] = FAST1_LUMA2_3433 | ((q->avg_g + 8) << 1) | ((q->avg_gr + 4) >> 2);
						q->out[q->p++] = (((q->avg_gr + 4) & 3) << 6) | ((q->avg_gb + 4) << 3) | ((q->va + 4) << 0);
					}
					else if ( q->va > -17 && q->va < 16 &&
						q->avg_gr > -17 && q->avg_gr < 16 &&
						q->avg_g  > -33 && q->avg_g  < 32 &&
						q->avg_gb > -17 && q->avg_gb < 16 ) {
						q->out[q->p++] = FAST1_LUMA3_5655 | ((q->avg_g + 32) >> 1);
						q->out[q->p++] = ((q->avg_g + 32) & 1) << 7 | ((q->avg_gr + 16) << 2) | ((q->avg_gb + 16) >> 3);
						q->out[q->p++] = (((q->avg_gb + 16) & 7) << 5) | ((q->va + 16) << 0);
					}
					else {
						q->out[q->p++] = FAST1_RGBA;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
						q->out[q->p++] = q->px.rgba.a;
					}
				}
				else {
					if ( q->avg_gr > -3 && q->avg_gr < 2 &&
						q->avg_g  > -5 && q->avg_g  < 4 &&
						q->avg_gb > -3 && q->avg_gb < 2 ) {
						q->out[q->p++] = FAST1_LUMA1_232 | ((q->avg_g + 4) << 4) | ((q->avg_gr + 2) << 2) | ((q->avg_gb + 2) << 0);
					}
					else if ( q->avg_gr > -9 && q->avg_gr < 8 &&
						q->avg_g  > -17 && q->avg_g  < 16 &&
						q->avg_gb > -9 && q->avg_gb < 8 ) {
						q->out[q->p++] = FAST1_LUMA2_454 | ((q->avg_g + 16) << 0);
						q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
					}
					else if ( q->avg_gr > -33 && q->avg_gr < 32 &&
						q->avg_g  > -65 && q->avg_g  < 64 &&
						q->avg_gb > -33 && q->avg_gb < 32 ) {
						q->out[q->p++] = FAST1_LUMA3_676 | ((q->avg_g + 64) >> 4);
						q->out[q->p++] = ((q->avg_g + 64) & 15) << 4 | ((q->avg_gr + 32) >> 2);
						q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
					}
					else {
						q->out[q->p++] = FAST1_RGB;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
					}
				}

				eop4:
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
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

				if (q->px.v == q->px_prev.v) {
					++q->run;
					goto eop3;
				}
				qoip_encode_run(q);
				qoip_gen_var_rgb(q);

				{
					if ( q->avg_gr > -3 && q->avg_gr < 2 &&
						q->avg_g  > -5 && q->avg_g  < 4 &&
						q->avg_gb > -3 && q->avg_gb < 2 ) {
						q->out[q->p++] = FAST1_LUMA1_232 | ((q->avg_g + 4) << 4) | ((q->avg_gr + 2) << 2) | ((q->avg_gb + 2) << 0);
					}
					else if ( q->avg_gr > -9 && q->avg_gr < 8 &&
						q->avg_g  > -17 && q->avg_g  < 16 &&
						q->avg_gb > -9 && q->avg_gb < 8 ) {
						q->out[q->p++] = FAST1_LUMA2_454 | ((q->avg_g + 16) << 0);
						q->out[q->p++] = ((q->avg_gr + 8) << 4) | ((q->avg_gb + 8) << 0);
					}
					else if ( q->avg_gr > -33 && q->avg_gr < 32 &&
						q->avg_g  > -65 && q->avg_g  < 64 &&
						q->avg_gb > -33 && q->avg_gb < 32 ) {
						q->out[q->p++] = FAST1_LUMA3_676 | ((q->avg_g + 64) >> 4);
						q->out[q->p++] = ((q->avg_g + 64) & 15) << 4 | ((q->avg_gr + 32) >> 2);
						q->out[q->p++] = (((q->avg_gr + 32) & 3) << 6) | ((q->avg_gb + 32) << 0);
					}
					else {
						q->out[q->p++] = FAST1_RGB;
						q->out[q->p++] = q->px.rgba.r;
						q->out[q->p++] = q->px.rgba.g;
						q->out[q->p++] = q->px.rgba.b;
					}
				}

				eop3:
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
				q->px_pos +=3;
			}
		}
	}
	qoip_encode_run(q);
	qoip_finish(q);
	*out_len=q->p;

	if(entropy)
		qoip_entropy(q->out, out_len, scratch, entropy);
	return 0;
}

static inline void qoip_decode_fast1_inner(qoip_working_t *q) {
	int b1, b2, b3, vg;
	if (q->run > 0)
		--q->run;
	else if (q->p < q->in_tot) {
		q->px_prev.v = q->px.v;
		if (q->px_pos >= q->stride && q->px_w<8192) {
			q->px_ref.rgba.r = (q->px.rgba.r + q->upcache[(q->px_w * 3) + 0] + 1) >> 1;
			q->px_ref.rgba.g = (q->px.rgba.g + q->upcache[(q->px_w * 3) + 1] + 1) >> 1;
			q->px_ref.rgba.b = (q->px.rgba.b + q->upcache[(q->px_w * 3) + 2] + 1) >> 1;
		}
		else
			q->px_ref.v = q->px_prev.v;

		b1 = q->in[q->p++];
		if(      (b1 & QOIP_MASK_1) == FAST1_LUMA1_232 ) {
			vg = ((b1 >> 4) & 7) - 4;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b1 >> 2) & 3) - 2;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b1 >> 0) & 3) - 2;
		}
		else if( (b1 & QOIP_MASK_3) == FAST1_LUMA2_454 ) {
			b2 = q->in[q->p++];
			vg = ((b1 >> 0) & 31) - 16;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 4) & 15) - 8;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 0) & 15) - 8;
		}
		else if( (b1 & QOIP_MASK_3) == FAST1_LUMA2_3433 ) {
			b2 = q->in[q->p++];
			vg = ((b1 >> 1) & 15) - 8;
			q->px.rgba.r = q->px_ref.rgba.r + vg + (((b1 & 1) << 2) | (b2 >> 6)) - 4;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b2 >> 3) & 7) - 4;
			q->px.rgba.a += ((b2 & 7) - 4);
		}
		else if( (b1 & QOIP_MASK_3) == FAST1_LUMA3_5655 ) {
			b2 = q->in[q->p++];
			b3 = q->in[q->p++];
			vg = (((b1 & 31) << 1) | (b2 >> 7)) - 32;
			q->px.rgba.r = q->px_ref.rgba.r + vg + ((b2 >> 2) & 31) - 16;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + (((b2 & 3) << 3) | (b3 >> 5)) - 16;
			q->px.rgba.a += ((b3 & 31) - 16);
		}
		else if( (b1 & QOIP_MASK_5) == FAST1_LUMA3_676 ) {
			b2 = q->in[q->p++];
			b3 = q->in[q->p++];
			vg = (((b1 & 7) << 4) | (b2 >> 4)) - 64;
			q->px.rgba.r = q->px_ref.rgba.r + vg + (((b2 & 15) << 2) | (b3 >> 6)) - 32;
			q->px.rgba.g = q->px_ref.rgba.g + vg;
			q->px.rgba.b = q->px_ref.rgba.b + vg + ((b3 >> 0) & 63) - 32;
		}
		else if( b1 == FAST1_RGB ) {
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
		}
		else if( b1 == FAST1_RGBA ) {
			q->px.rgba.r = q->in[q->p++];
			q->px.rgba.g = q->in[q->p++];
			q->px.rgba.b = q->in[q->p++];
			q->px.rgba.a = q->in[q->p++];
		}
		else if( b1 == FAST1_RUN2 )
			q->run = q->in[q->p++] + 21;
		else
			q->run = b1 - FAST1_RUN1;
	}
	if(q->px_w<8192) {
		q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
		q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
		q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
	}
}

int qoip_decode_fast1(qoip_working_t *q) {
	if(q->channels==4) {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_fast1_inner(q);
				*(qoip_rgba_t*)(q->out + q->px_pos) = q->px;
				q->px_pos += 4;
			}
		}
	}
	else {
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				qoip_decode_fast1_inner(q);
				q->out[q->px_pos + 0] = q->px.rgba.r;
				q->out[q->px_pos + 1] = q->px.rgba.g;
				q->out[q->px_pos + 2] = q->px.rgba.b;
				q->px_pos += 3;
			}
		}
	}
	return 0;
}
