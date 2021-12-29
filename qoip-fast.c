/* fastpath implementations. Included by QOIP_C only, after necessary includes

* Encode functions assume header has been written, but footer needs to be written

*/
enum{DEFAULT_OP_DIFF=0x00, DEFAULT_OP_LUMA=0x40, DEFAULT_OP_RGB3=0x80, DEFAULT_OP_RUN1=0xc0, DEFAULT_OP_RUN2=0xdc, DEFAULT_OP_A=0xdd, DEFAULT_OP_RGBA=0xde, DEFAULT_OP_RGB=0xdf, DEFAULT_OP_INDEX5=0xe0};
int qoip_encode_default(qoip_working_t *q, size_t *out_len) {
	int index_pos;
	size_t px_pos;
	q->run1_len = 28;
	q->run1_opcode=DEFAULT_OP_RUN1;
	q->run2_opcode=DEFAULT_OP_RUN2;
	if(q->channels==4) {
		for (px_pos = 0; px_pos < q->px_len; px_pos += 4) {
			q->px = *(qoip_rgba_t *)(q->in + px_pos);

			if (q->px.v == q->px_prev.v) {
				++q->run;/* Accumulate as much RLE as there is */
				q->px_prev = q->px;
				continue;
			}
			qoip_encode_run(q);
			index_pos = QOIP_COLOR_HASH(q->px) & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = DEFAULT_OP_INDEX5 | index_pos;
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
					q->out[q->p++] = DEFAULT_OP_DIFF | (q->vr+2) << 4 | (q->vg+2) << 2 | (q->vb+2);
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
					q->out[q->p++] = DEFAULT_OP_LUMA    | (q->vg   + 32);
					q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
				}
				else if (//rgb3
					q->vr > -65 && q->vr < 64 &&
					q->vb > -65 && q->vb < 64
				) {
					q->out[q->p++] = DEFAULT_OP_RGB3           | ((q->vr + 64) >> 1);
					q->out[q->p++] = q->px.rgba.g;
					q->out[q->p++] = (((q->vr + 64) & 1) << 7) | (q->vb + 64);
				}
				else{//rgb
					q->out[q->p++] = DEFAULT_OP_RGB;
					q->out[q->p++] = q->px.rgba.r;
					q->out[q->p++] = q->px.rgba.g;
					q->out[q->p++] = q->px.rgba.b;
				}
			}
			else if ( q->vr == 0 && q->vg == 0 && q->vb == 0 ) {//OP_A
				q->out[q->p++] = DEFAULT_OP_A;
				q->out[q->p++] = q->px.rgba.a;
			}
			else {//OP_RGBA
				q->out[q->p++] = DEFAULT_OP_RGBA;
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
				q->px_prev = q->px;
				continue;
			}
			qoip_encode_run(q);
			index_pos = QOIP_COLOR_HASH(q->px) & 31;
			if (q->index[index_pos].v == q->px.v) {
				q->out[q->p++] = DEFAULT_OP_INDEX5 | index_pos;
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
				q->out[q->p++] = DEFAULT_OP_DIFF | (q->vr+2) << 4 | (q->vg+2) << 2 | (q->vb+2);
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
				q->out[q->p++] = DEFAULT_OP_LUMA    | (q->vg   + 32);
				q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
			}
			else if (//rgb3
				q->vr > -65 && q->vr < 64 &&
				q->vb > -65 && q->vb < 64
			) {
				q->out[q->p++] = DEFAULT_OP_RGB3           | ((q->vr + 64) >> 1);
				q->out[q->p++] = q->px.rgba.g;
				q->out[q->p++] = (((q->vr + 64) & 1) << 7) | (q->vb + 64);
			}
			else{//rgb
				q->out[q->p++] = DEFAULT_OP_RGB;
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

