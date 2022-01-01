/* fastpath implementations. Included by QOIP_C only, after necessary includes

* Encode functions assume header has been written, but footer needs to be written

*/

/* propc */
enum{PROPC_OP_DIFF=0x00, PROPC_OP_LUMA=0x40, PROPC_OP_RGB3=0x80, PROPC_OP_INDEX5=0xc0, PROPC_OP_A=0xe0, PROPC_OP_RGB=0xe1, PROPC_OP_RGBA=0xe2, PROPC_OP_RUN2=0xe3, PROPC_OP_RUN1=0xe4};

int qoip_encode_propc(qoip_working_t *q, size_t *out_len) {
	int index_pos;
	size_t px_pos;
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			q->px = *(qoip_rgba_t *)(q->in + px_pos);

			if (q->px.v == q->px_prev.v) {
				++q->run;/* Accumulate as much RLE as there is */
				continue;
			}
			qoip_encode_run(q);
			index_pos = QOIP_COLOR_HASH(q->px) & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = PROPC_OP_INDEX5 | index_pos;
				q->px_prev = q->px;
				continue;
			}
			q->index[index_pos] = q->px;
			q->vr = q->px.rgba.r - q->px_prev.rgba.r;
			q->vg = q->px.rgba.g - q->px_prev.rgba.g;
			q->vb = q->px.rgba.b - q->px_prev.rgba.b;
			if(q->px.rgba.a == q->px_prev.rgba.a) {
				if (
					q->vr > -3 && q->vr < 2 &&
					q->vg > -3 && q->vg < 2 &&
					q->vb > -3 && q->vb < 2
				) {
					q->out[q->p++] = PROPC_OP_DIFF | (q->vr+2) << 4 | (q->vg+2) << 2 | (q->vb+2);
					q->px_prev = q->px;
					continue;
				}
				q->vg_r = q->vr - q->vg;
				q->vg_b = q->vb - q->vg;
				if (
					q->vg_r >  -9 && q->vg_r <  8 &&
					q->vg   > -33 && q->vg   < 32 &&
					q->vg_b >  -9 && q->vg_b <  8
				) {
					q->out[q->p++] = PROPC_OP_LUMA    | (q->vg   + 32);
					q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
				}
				else if (
					q->vr > -65 && q->vr < 64 &&
					q->vb > -65 && q->vb < 64
				) {
					q->out[q->p++] = PROPC_OP_RGB3           | ((q->vr + 64) >> 1);
					q->out[q->p++] = q->px.rgba.g;
					q->out[q->p++] = (((q->vr + 64) & 1) << 7) | (q->vb + 64);
				}
				else{
					q->out[q->p++] = PROPC_OP_RGB;
					q->out[q->p++] = q->px.rgba.r;
					q->out[q->p++] = q->px.rgba.g;
					q->out[q->p++] = q->px.rgba.b;
				}
			}
			else if ( q->vr == 0 && q->vg == 0 && q->vb == 0 ) {
				q->out[q->p++] = PROPC_OP_A;
				q->out[q->p++] = q->px.rgba.a;
			}
			else {
				q->out[q->p++] = PROPC_OP_RGBA;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
				q->out[q->p++] = q->px.rgba.a;
			}
			q->px_prev = q->px;
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			q->px.rgba.r = q->in[px_pos + 0];
			q->px.rgba.g = q->in[px_pos + 1];
			q->px.rgba.b = q->in[px_pos + 2];

			if (q->px.v == q->px_prev.v) {
				++q->run;/* Accumulate as much RLE as there is */
				continue;
			}
			qoip_encode_run(q);
			index_pos = QOIP_COLOR_HASH(q->px) & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = PROPC_OP_INDEX5 | index_pos;
				q->px_prev = q->px;
				continue;
			}
			q->index[index_pos] = q->px;
			q->vr = q->px.rgba.r - q->px_prev.rgba.r;
			q->vg = q->px.rgba.g - q->px_prev.rgba.g;
			q->vb = q->px.rgba.b - q->px_prev.rgba.b;
			if (
				q->vr > -3 && q->vr < 2 &&
				q->vg > -3 && q->vg < 2 &&
				q->vb > -3 && q->vb < 2
			) {
				q->out[q->p++] = PROPC_OP_DIFF | (q->vr+2) << 4 | (q->vg+2) << 2 | (q->vb+2);
				q->px_prev = q->px;
				continue;
			}
			q->vg_r = q->vr - q->vg;
			q->vg_b = q->vb - q->vg;
			if (
				q->vg_r >  -9 && q->vg_r <  8 &&
				q->vg   > -33 && q->vg   < 32 &&
				q->vg_b >  -9 && q->vg_b <  8
			) {
				q->out[q->p++] = PROPC_OP_LUMA    | (q->vg   + 32);
				q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
			}
			else if (
				q->vr > -65 && q->vr < 64 &&
				q->vb > -65 && q->vb < 64
			) {
				q->out[q->p++] = PROPC_OP_RGB3           | ((q->vr + 64) >> 1);
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = (((q->vr + 64) & 1) << 7) | (q->vb + 64);
			}
			else{
				q->out[q->p++] = PROPC_OP_RGB;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
			}
			q->px_prev = q->px;
		}
	}
	qoip_encode_run(q);
	qoip_finish(q);
	*out_len=q->p;
	return 0;
}

static inline void qoip_decode_propc_inner(qoip_working_t *q) {
	if      ((q->in[q->p] & MASK_2) == PROPC_OP_DIFF) {
		q->px.rgba.r += ((q->in[q->p] >> 4) & 0x03) - 2;
		q->px.rgba.g += ((q->in[q->p] >> 2) & 0x03) - 2;
		q->px.rgba.b += ( q->in[q->p]       & 0x03) - 2;
		++q->p;
	}
	else if ((q->in[q->p] & MASK_2) == PROPC_OP_LUMA) {
		int b1 = q->in[q->p++];
		int b2 = q->in[q->p++];
		int vg = (b1 & 0x3f) - 32;
		q->px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
		q->px.rgba.g += vg;
		q->px.rgba.b += vg - 8 +  (b2       & 0x0f);
	}
	else if ((q->in[q->p] & MASK_2) == PROPC_OP_RGB3) {
		int b1 = q->in[q->p++];
		int b2 = q->in[q->p++];
		int b3 = q->in[q->p++];
		q->px.rgba.r += (((b1 & 0x3f) << 1) | ((b3) >> 7)) - 64;
		q->px.rgba.g  = b2;
		q->px.rgba.b += (b3 & 0x7f) - 64;
	}
	else if ((q->in[q->p] & MASK_3) == PROPC_OP_INDEX5) {
		q->px = q->index[q->in[q->p++] & 0x1f];
	}
	else if (q->in[q->p] == PROPC_OP_RUN2) {
		++q->p;
		q->run = q->in[q->p++] + q->run1_len;
	}
	else if (q->in[q->p] == PROPC_OP_RGB) {
		++q->p;
		q->px.rgba.r = q->in[q->p++];
		q->px.rgba.g = q->in[q->p++];
		q->px.rgba.b = q->in[q->p++];
	}
	else if (q->in[q->p] == PROPC_OP_RGBA) {
		++q->p;
		q->px.rgba.r = q->in[q->p++];
		q->px.rgba.g = q->in[q->p++];
		q->px.rgba.b = q->in[q->p++];
		q->px.rgba.a = q->in[q->p++];
	}
	else if (q->in[q->p] == PROPC_OP_A) {
		++q->p;
		q->px.rgba.a = q->in[q->p++];
	}
	else{
		q->run = q->in[q->p++] - q->run1_opcode;
	}
	q->index[QOIP_COLOR_HASH(q->px) & 31] = q->px;
}

int qoip_decode_propc(qoip_working_t *q, size_t data_len) {
	size_t px_pos;
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len)
				qoip_decode_propc_inner(q);
			*(qoip_rgba_t*)(q->out + px_pos) = q->px;
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len)
				qoip_decode_propc_inner(q);
			q->out[px_pos + 0] = q->px.rgba.r;
			q->out[px_pos + 1] = q->px.rgba.g;
			q->out[px_pos + 2] = q->px.rgba.b;
		}
	}
	return 0;
}

/* deltax */
enum{DELTAX_OP_LUMA1=0x00, DELTAX_OP_LUMA2=0x80, DELTAX_OP_INDEX5=0xc0, DELTAX_OP_LUMA3=0xe0, DELTAX_OP_LUMA3A=0xe8, DELTAX_OP_INDEX8=0xf0, DELTAX_OP_RGB=0xf1, DELTAX_OP_RGBA=0xf2, DELTAX_OP_RUN2=0xf3, DELTAX_OP_RUN1=0xf4};

int qoip_encode_deltax(qoip_working_t *q, size_t *out_len) {
	int index_pos;
	size_t px_pos;
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			q->px = *(qoip_rgba_t *)(q->in + px_pos);

			if (q->px.v == q->px_prev.v) {
				++q->run;/* Accumulate as much RLE as there is */
				continue;
			}
			qoip_encode_run(q);

			q->hash = QOIP_COLOR_HASH(q->px) & 255;
			index_pos = q->hash & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = DELTAX_OP_INDEX5 | index_pos;
				q->px_prev = q->px;
				continue;
			}
			q->index[index_pos] = q->px;

			q->vr = q->px.rgba.r - q->px_prev.rgba.r;
			q->vg = q->px.rgba.g - q->px_prev.rgba.g;
			q->vb = q->px.rgba.b - q->px_prev.rgba.b;
			q->va = q->px.rgba.a - q->px_prev.rgba.a;
			q->vg_r = q->vr - q->vg;
			q->vg_b = q->vb - q->vg;

			if(q->va != 0) {
				if(q->index2[q->hash].v == q->px.v) {
					q->out[q->p++] = DELTAX_OP_INDEX8;
					q->out[q->p++] = q->hash;
				}
				else if (
				q->va   > -17 && q->va   < 16 &&
				q->vg_r >  -9 && q->vg_r <  8 &&
				q->vg   > -33 && q->vg   < 32 &&
				q->vg_b >  -9 && q->vg_b <  8
				) {
					q->index2[q->hash].v = q->px.v;
					q->out[q->p++] = DELTAX_OP_LUMA3A | ((q->vg   + 32) >> 3);
					q->out[q->p++] = (((q->vg + 32) & 0x07) << 5) | (q->va +  16);
					q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
				}
				else {
					q->index2[q->hash].v = q->px.v;
					q->out[q->p++] = DELTAX_OP_RGBA;
					q->out[q->p++] = q->px.rgba.r;
					q->out[q->p++] = q->px.rgba.g;
					q->out[q->p++] = q->px.rgba.b;
					q->out[q->p++] = q->px.rgba.a;
				}
				q->px_prev = q->px;
				continue;
			}

			if (
				q->vg_r > -3 && q->vg_r < 2 &&
				q->vg >   -5 && q->vg   < 4 &&
				q->vg_b > -3 && q->vg_b < 2
			) {
				q->out[q->p++] = DELTAX_OP_LUMA1 | ((q->vg+4) << 4) | ((q->vg_r+2) << 2) | (q->vg_b+2);
				q->px_prev = q->px;
				q->index2[q->hash].v = q->px.v;
				continue;
			}

			if(q->index2[q->hash].v == q->px.v) {
				q->out[q->p++] = DELTAX_OP_INDEX8;
				q->out[q->p++] = q->hash;
				q->px_prev = q->px;
				continue;
			}

			if (
				q->vg_r >  -9 && q->vg_r <  8 &&
				q->vg   > -33 && q->vg   < 32 &&
				q->vg_b >  -9 && q->vg_b <  8
			) {
				q->out[q->p++] = DELTAX_OP_LUMA2    | (q->vg   + 32);
				q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
			}
			else if (
				q->vg_r > -33 && q->vg_r < 32 &&
				q->vg   > -65 && q->vg   < 64 &&
				q->vg_b > -33 && q->vg_b < 32
			) {
				q->out[q->p++] = DELTAX_OP_LUMA3  | ((q->vg + 64) >> 4);
				q->out[q->p++] = (((q->vg   + 64) & 0x0f) << 4) | ((q->vg_b + 32) >> 2);
				q->out[q->p++] = (((q->vg_b + 32) & 0x03) << 6) | ( q->vg_r + 32      );
			}
			else{
				q->out[q->p++] = DELTAX_OP_RGB;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
			}
			q->px_prev = q->px;
			q->index2[q->hash].v = q->px.v;
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			q->px.rgba.r = q->in[px_pos + 0];
			q->px.rgba.g = q->in[px_pos + 1];
			q->px.rgba.b = q->in[px_pos + 2];

			if (q->px.v == q->px_prev.v) {
				++q->run;/* Accumulate as much RLE as there is */
				continue;
			}
			qoip_encode_run(q);

			q->hash = QOIP_COLOR_HASH(q->px) & 255;
			index_pos = q->hash & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = DELTAX_OP_INDEX5 | index_pos;
				q->px_prev = q->px;
				continue;
			}
			q->index[index_pos] = q->px;

			q->vr = q->px.rgba.r - q->px_prev.rgba.r;
			q->vg = q->px.rgba.g - q->px_prev.rgba.g;
			q->vb = q->px.rgba.b - q->px_prev.rgba.b;
			q->vg_r = q->vr - q->vg;
			q->vg_b = q->vb - q->vg;

			if (
				q->vg_r > -3 && q->vg_r < 2 &&
				q->vg >   -5 && q->vg   < 4 &&
				q->vg_b > -3 && q->vg_b < 2
			) {
				q->out[q->p++] = DELTAX_OP_LUMA1 | ((q->vg+4) << 4) | ((q->vg_r+2) << 2) | (q->vg_b+2);
				q->px_prev = q->px;
				q->index2[q->hash].v = q->px.v;
				continue;
			}

			if(q->index2[q->hash].v == q->px.v) {
				q->out[q->p++] = DELTAX_OP_INDEX8;
				q->out[q->p++] = q->hash;
				q->px_prev = q->px;
				continue;
			}

			if (
				q->vg_r >  -9 && q->vg_r <  8 &&
				q->vg   > -33 && q->vg   < 32 &&
				q->vg_b >  -9 && q->vg_b <  8
			) {
				q->out[q->p++] = DELTAX_OP_LUMA2    | (q->vg   + 32);
				q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
			}
			else if (
				q->vg_r > -33 && q->vg_r < 32 &&
				q->vg   > -65 && q->vg   < 64 &&
				q->vg_b > -33 && q->vg_b < 32
			) {
				q->out[q->p++] = DELTAX_OP_LUMA3  | ((q->vg + 64) >> 4);
				q->out[q->p++] = (((q->vg   + 64) & 0x0f) << 4) | ((q->vg_b + 32) >> 2);
				q->out[q->p++] = (((q->vg_b + 32) & 0x03) << 6) | ( q->vg_r + 32      );
			}
			else{
				q->out[q->p++] = DELTAX_OP_RGB;
				q->out[q->p++] = q->px.rgba.r;
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = q->px.rgba.b;
			}
			q->px_prev = q->px;
			q->index2[q->hash].v = q->px.v;
		}
	}
	qoip_encode_run(q);
	qoip_finish(q);
	*out_len=q->p;
	return 0;
}

static inline void qoip_decode_deltax_inner(qoip_working_t *q) {
	if      ((q->in[q->p] & MASK_1) == DELTAX_OP_LUMA1) {
		int b1 = q->in[q->p++];
		int vg = ((b1 >> 4) - 4);
		q->px.rgba.r += vg - 2 + ((b1 >> 2) & 0x03);
		q->px.rgba.g += vg;
		q->px.rgba.b += vg - 2 + ((b1     ) & 0x03);
	}
	else if ((q->in[q->p] & MASK_2) == DELTAX_OP_LUMA2) {
		int b1 = q->in[q->p++];
		int b2 = q->in[q->p++];
		int vg = (b1 & 0x3f) - 32;
		q->px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
		q->px.rgba.g += vg;
		q->px.rgba.b += vg - 8 +  (b2       & 0x0f);
	}
	else if ((q->in[q->p] & MASK_3) == DELTAX_OP_INDEX5)
		q->px = q->index[q->in[q->p++] & 0x1f];
	else if (q->in[q->p] == DELTAX_OP_INDEX8) {
		++q->p;
		q->px = q->index2[q->in[q->p++]];
	}
	else if ((q->in[q->p] & MASK_5) == DELTAX_OP_LUMA3) {
		int b1 = q->in[q->p++];
		int b2 = q->in[q->p++];
		int b3 = q->in[q->p++];
		int vg = (((b1 & 0x07) << 4) | (b2 >> 4)) - 64;
		q->px.rgba.r += vg - 32 + (b3 & 0x3f);
		q->px.rgba.g += vg;
		q->px.rgba.b += vg - 32 + (((b2 & 0x0f) << 2) | ((b3 >> 6) & 0x03));
	}
	else if ((q->in[q->p] & MASK_5) == DELTAX_OP_LUMA3A) {
		int b1 = q->in[q->p++];
		int b2 = q->in[q->p++];
		int b3 = q->in[q->p++];
		int vg = (((b1 & 0x07) << 3) | (b2 >> 5)) - 32;
		q->px.rgba.r += vg - 8 + ((b3 >> 4) & 0x0f);
		q->px.rgba.g += vg;
		q->px.rgba.b += vg - 8 +  (b3       & 0x0f);
		q->px.rgba.a += (b2 & 0x1f) - 16;
	}
	else if (q->in[q->p] == DELTAX_OP_RUN2) {
		++q->p;
		q->run = q->in[q->p++] + q->run1_len;
	}
	else if (q->in[q->p] == DELTAX_OP_RGB) {
		++q->p;
		q->px.rgba.r = q->in[q->p++];
		q->px.rgba.g = q->in[q->p++];
		q->px.rgba.b = q->in[q->p++];
	}
	else if (q->in[q->p] == DELTAX_OP_RGBA) {
		++q->p;
		q->px.rgba.r = q->in[q->p++];
		q->px.rgba.g = q->in[q->p++];
		q->px.rgba.b = q->in[q->p++];
		q->px.rgba.a = q->in[q->p++];
	}
	else {
		q->run = q->in[q->p++] - q->run1_opcode;
	}
	q->index[QOIP_COLOR_HASH(q->px) & 31] = q->px;
	q->index2[QOIP_COLOR_HASH(q->px) & 255] = q->px;
}

int qoip_decode_deltax(qoip_working_t *q, size_t data_len) {
	size_t px_pos;
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len)
				qoip_decode_deltax_inner(q);
			*(qoip_rgba_t*)(q->out + px_pos) = q->px;
		}
	}
	else {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 3) {
			if (q->run > 0)
				--q->run;
			else if (q->p < data_len)
				qoip_decode_deltax_inner(q);
			q->out[px_pos + 0] = q->px.rgba.r;
			q->out[px_pos + 1] = q->px.rgba.g;
			q->out[px_pos + 2] = q->px.rgba.b;
		}
	}
	return 0;
}
