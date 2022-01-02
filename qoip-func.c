/* Op encode/decode functions used by qoip.h implementation. Included by QOIP_C only

* qoip_enc_* is the encoder for OP_*, qoip_dec_* is the decoder
* The encode functions detect if an op can be used and encodes it if it can. If
  the op is used 1 is returned so qoip_encode knows to proceed to the next pixel
* The decode functions are called when qoip_decode has determined the op was
  used, no detection necessary
*/

/* === Hash cache index functions */
/* This function encodes all index1_* ops */
static int qoip_enc_index(qoip_working_t *q, u8 opcode) {
	int index_pos = q->hash & q->index1_maxval;
	if (q->index[index_pos].v == q->px.v) {
		q->out[q->p++] = opcode | index_pos;
		return 1;
	}
	q->index[index_pos] = q->px;
	return 0;
}
static void qoip_dec_index(qoip_working_t *q) {
	q->px = q->index[q->in[q->p++] & q->index1_maxval];
}

static int qoip_enc_index8(qoip_working_t *q, u8 opcode) {
	if (q->index2[q->hash].v == q->px.v) {
		q->out[q->p++] = opcode;
		q->out[q->p++] = q->hash;
		return 1;
	}
	return 0;
}
static void qoip_dec_index8(qoip_working_t *q) {
	++q->p;
	q->px = q->index2[q->in[q->p++]];
}

/* === Length 1 RGB delta functions */
static int qoip_enc_delta(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vr > -2 && q->vr < 2 &&
		q->vg > -2 && q->vg < 2 &&
		q->vb > -2 && q->vb < 2
	) {
		q->out[q->p++] = opcode | (((q->vb + 1) * 9) + ((q->vg + 1) * 3) + (q->vr + 1));
		return 1;
	}
	else if (
		q->va == 0 &&
		((q->vr<0?-q->vr:q->vr)+(q->vg<0?-q->vg:q->vg)+(q->vb<0?-q->vb:q->vb))==2
	) {
		if(q->vr==2)
			q->out[q->p++] = opcode | 13;
		else if(q->vr==-2)
			q->out[q->p++] = opcode | 27;
		else if(q->vg==2)
			q->out[q->p++] = opcode | 28;
		else if(q->vg==-2)
			q->out[q->p++] = opcode | 29;
		else if(q->vb==2)
			q->out[q->p++] = opcode | 30;
		else
			q->out[q->p++] = opcode | 31;
		return 1;
	}
	return 0;
}
static void qoip_dec_delta(qoip_working_t *q) {
	int b1=q->in[q->p++]&31;
	switch(b1){
		case 13:
			q->px.rgba.r += 2;
			break;
		case 27:
			q->px.rgba.r -= 2;
			break;
		case 28:
			q->px.rgba.g += 2;
			break;
		case 29:
			q->px.rgba.g -= 2;
			break;
		case 30:
			q->px.rgba.b += 2;
			break;
		case 31:
			q->px.rgba.b -= 2;
			break;
		default:
			q->px.rgba.r += ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.g += ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.b += ((b1 % 3) - 1);
			break;
	}
}

static int qoip_enc_diff1_222(qoip_working_t *q, u8 opcode) {
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
static void qoip_dec_diff1_222(qoip_working_t *q) {
	q->px.rgba.r += ((q->in[q->p] >> 4) & 0x03) - 2;
	q->px.rgba.g += ((q->in[q->p] >> 2) & 0x03) - 2;
	q->px.rgba.b += ( q->in[q->p]       & 0x03) - 2;
	++q->p;
}

static int qoip_enc_luma1_232(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r > -3 && q->vg_r < 2 &&
		q->vg >   -5 && q->vg <   4 &&
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

/* === Length 2 RGB delta functions */
static int qoip_enc_luma2_454(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r >  -9 && q->vg_r <  8 &&
		q->vg   > -17 && q->vg   < 16 &&
		q->vg_b >  -9 && q->vg_b <  8
	) {
		q->out[q->p++] = opcode             | (q->vg   + 16);
		q->out[q->p++] = (q->vg_r + 8) << 4 | (q->vg_b +  8);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma2_454(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b1 & 0x1f) - 16;
	q->px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 8 +  (b2       & 0x0f);
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

/* === Length 3 RGB delta functions */
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

static int qoip_enc_luma3_686(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r >  -33 && q->vg_r <  32 &&
		q->vg_b >  -33 && q->vg_b <  32
	) {
		q->out[q->p++] = opcode                      | ((q->vg_r + 32) >> 2);
		q->out[q->p++] = (((q->vg_r + 32) & 3) << 6) |  (q->vg_b + 32);
		q->out[q->p++] = q->vg + 128;
		return 1;
	}
	return 0;
}
static void qoip_dec_luma3_686(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = b3 - 128;
	q->px.rgba.r += vg - 32 + (((b1 & 0x0f) << 2) | ((b2) >> 6));
	q->px.rgba.b += vg - 32 + (b2 & 0x3f);
	q->px.rgba.g += vg;
}

static int qoip_enc_luma3_787(qoip_working_t *q, u8 opcode) {
	if (
		q->va == 0 &&
		q->vg_r >  -65 && q->vg_r <  64 &&
		q->vg_b >  -65 && q->vg_b <  64
	) {
		q->out[q->p++] = opcode                      | ((q->vg_r + 64) >> 1);
		q->out[q->p++] = q->vg + 128;
		q->out[q->p++] = (((q->vg_r + 64) & 1) << 7) |  (q->vg_b + 64);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma3_787(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = b2 - 128;
	q->px.rgba.r += vg - 64 + (((b1 & 0x3f) << 1) | ((b3) >> 7));
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 64 + (b3 & 0x7f);
}

/* === length 1 RGBA delta functions */
static int qoip_enc_deltaa(qoip_working_t *q, u8 opcode) {
	if (
		(q->va == -1 || q->va == 1) &&
		q->vr > -2 && q->vr < 2 &&
		q->vg > -2 && q->vg < 2 &&
		q->vb > -2 && q->vb < 2
	) {
		q->out[q->p++] = opcode | (q->va==1?32:0) | (((q->vb+1)*9)+((q->vg+1)*3)+(q->vr+1));
		return 1;
	}
	else if (/*encode small changes in a, -6..-2, 2..6*/
		q->vr == 0 && q->vg == 0 && q->vb == 0 &&
		q->va > -7 && q->va < 7
	) {
		if(q->va>0)
			q->out[q->p++] = opcode | 32 | (25 + q->va);
		else
			q->out[q->p++] = opcode |      (25 - q->va);
		return 1;
	}
	return 0;
}
static void qoip_dec_deltaa(qoip_working_t *q) {
	int b1=q->in[q->p++];
	switch(b1&31){
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
			if(b1&32)
				q->px.rgba.a += ((b1&31)-25);
			else
				q->px.rgba.a -= ((b1&31)-25);
			break;
		default:
			q->px.rgba.a += (b1 & 32) ? 1 : -1;
			b1 &= 31;
			q->px.rgba.r += ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.g += ((b1 % 3) - 1);
			b1/=3;
			q->px.rgba.b += ((b1 % 3) - 1);
			break;
	}
}

/* === Length 2 RGBA delta functions */
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

static int qoip_enc_luma2_3433(qoip_working_t *q, u8 opcode) {
	if (
		q->va   >  -5 && q->va   <  4 &&
		q->vg_r >  -5 && q->vg_r <  4 &&
		q->vg   >  -9 && q->vg   <  8 &&
		q->vg_b >  -5 && q->vg_b <  4
	) {/* tttrrrbb baaagggg */
		q->out[q->p++] = opcode      | (q->vg_r + 4) << 2 | (q->vg_b + 4) >> 1;
		q->out[q->p++] = ((q->vg_b + 4) & 1) << 7 | (q->va + 4) << 4 | (q->vg + 8);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma2_3433(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int vg = (b2 & 0xf) - 8;
	q->px.rgba.r += vg - 4 + ((b1 >> 2) & 7);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 4 + ((b1 & 3) << 1) + (b2 >> 7);
	q->px.rgba.a += ((b2 >> 4) & 7) - 4;
}

/* === Length 3 RGBA delta functions */
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

static int qoip_enc_luma3_5654(qoip_working_t *q, u8 opcode) {
	if (
		q->va   >  -9 && q->va    < 8 &&
		q->vg_r > -17 && q->vg_r < 16 &&
		q->vg   > -33 && q->vg   < 32 &&
		q->vg_b > -17 && q->vg_b < 16
	) {/* ttttaaaa rrrrrbbb bbgggggg */
		q->out[q->p++] = opcode | (q->va   + 8);
		q->out[q->p++] = (q->vg_r + 16) << 3 | ((q->vg_b + 16) >> 2);
		q->out[q->p++] = ((q->vg_b + 16) & 3) << 6 | (q->vg + 32);
		return 1;
	}
	return 0;
}
static void qoip_dec_luma3_5654(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int vg = (b3 & 0x3f) - 32;
	q->px.rgba.r += vg - 16 + ((b2 >> 3) & 0x1f);
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 16 +  (((b2 & 7) << 2) | (b3 >> 6));
	q->px.rgba.a += (b1 & 0x0f) - 8;
}

/* === Length 4 RGBA delta functions */
static int qoip_enc_luma4_7777(qoip_working_t *q, u8 opcode) {
	if (
		q->va   > -65 && q->va   < 64 &&
		q->vg_r > -65 && q->vg_r < 64 &&
		q->vg   > -65 && q->vg   < 64 &&
		q->vg_b > -65 && q->vg_b < 64
	) {/* ttttrrrr rrrggggg ggbbbbbb baaaaaaa */
		q->out[q->p++] = opcode                      | ((q->vg_r + 64) >> 3);
		q->out[q->p++] = (((q->vg_r + 64) & 7) << 5) | ((q->vg   + 64) >> 2);
		q->out[q->p++] = (((q->vg   + 64) & 3) << 6) | ((q->vg_b + 64) >> 1);
		q->out[q->p++] = (((q->vg_b + 64) & 1) << 7) | ((q->va   + 64)     );
		return 1;
	}
	return 0;
}
static void qoip_dec_luma4_7777(qoip_working_t *q) {
	int b1 = q->in[q->p++];
	int b2 = q->in[q->p++];
	int b3 = q->in[q->p++];
	int b4 = q->in[q->p++];
	int vg = (((b2 & 0x1f) << 2) | (b3 >> 6)) - 64;
	q->px.rgba.r += vg - 64 + (((b1 & 0x0f) << 3) | (b2 >> 5));
	q->px.rgba.g += vg;
	q->px.rgba.b += vg - 64 + (((b3 & 0x3f) << 1) | (b4 >> 7));
	q->px.rgba.a += (b4 & 0x7f) - 64;
}
