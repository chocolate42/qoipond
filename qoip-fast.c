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

/* Effort 0 */
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
				if ( q->va == 0 &&
					q->avg_g   > -5 && q->avg_g   < 0 &&
					q->avg_gr > -2 && q->avg_gr < 3 &&
					q->avg_gb > -2 && q->avg_gb < 3 ) {
					q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
					goto eop4;
				}
				else if ( q->va == 0 &&
					q->avg_g   > -1 && q->avg_g   < 4 &&
					q->avg_gr > -3 && q->avg_gr < 2 &&
					q->avg_gb > -3 && q->avg_gb < 2 ) {
					q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
					goto eop4;
				}

				/*Index2*/
				if (q->index2[q->hash & 1023].v == q->px.v) {
					q->out[q->p++] = E0_INDEX10 | ((q->hash & 1023) >> 8);
					q->out[q->p++] = q->hash & 255;
					goto eop4;
				}

				/*other*/
				//G 3..8, RB 2..8, A 0,2..6
				if(q->va!=0) {//E0_LUMA4_6866=0xeC, E0_LUMA2_2322=0xf0, E0_LUMA3_4544=0xf2, E0_A=0xf4,  E0_RGBA=0xf6
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
				else {//E0_RGB=0xf5, E0_LUMA2_464=0x80, E0_LUMA3_676=0xe0,
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
				if ( q->va == 0 &&
					q->avg_g   > -5 && q->avg_g   < 0 &&
					q->avg_gr > -2 && q->avg_gr < 3 &&
					q->avg_gb > -2 && q->avg_gb < 3 ) {
					q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 1) << 2 | (q->avg_gb + 1);
					goto eop3;
				}
				else if ( q->va == 0 &&
					q->avg_g   > -1 && q->avg_g   < 4 &&
					q->avg_gr > -3 && q->avg_gr < 2 &&
					q->avg_gb > -3 && q->avg_gb < 2 ) {
					q->out[q->p++] = E0_LUMA1_232B | (q->avg_g + 4) << 4 | (q->avg_gr + 2) << 2 | (q->avg_gb + 2);
					goto eop3;
				}

				/*Index2*/
				if (q->index2[q->hash & 1023].v == q->px.v) {
					q->out[q->p++] = E0_INDEX10 | ((q->hash & 1023) >> 8);
					q->out[q->p++] = q->hash & 255;
					goto eop3;
				}

				/*other*/
				//G 3..8, RB 2..8, A 0,2..6
				{//E0_RGB=0xf5, E0_LUMA2_464=0x80, E0_LUMA3_676=0xe0,
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
