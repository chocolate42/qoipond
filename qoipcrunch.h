/* SPDX-License-Identifier: MIT */
/* qoipcrunch.h - Encode wrapper for qoip_encode to search for better combinations

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
*/

#ifndef QOIPCRUNCH_H
#define QOIPCRUNCH_H
#include "qoip.h"
#include "qoipcrunch-list.h"
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*Controller, parses options and calls appropriate crunch function */
int qoipcrunch_encode        (const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, void *scratch, int threads, int entropy);
/*Crunch functions*/
int qoipcrunch_encode_custom (const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, void *tmp, int threads, int entropy);
int qoipcrunch_encode_smart  (const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level,    void *tmp, int threads, int entropy);
int qoipcrunch_encode_smarter(const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level,    void *tmp, int threads, int entropy);

#ifdef __cplusplus
}
#endif
#endif /* QOIPCRUNCH_H */

#ifdef QOIPCRUNCH_C
#include "lz4.h"
#include "zstd.h"
#include <omp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define QOIP_MAX_THREADS 64

static int qoip_effortlevel(char *effort) {
	if(     strcmp(effort, "0")==0 || !effort)
		return 0;
	else if(strcmp(effort, "1")==0)
		return 1;
	else if(strcmp(effort, "2")==0)
		return 2;
	else if(strcmp(effort, "3")==0)
		return 3;
	else if(strcmp(effort, "4")==0)
		return 4;
	else if(strcmp(effort, "5")==0)
		return 5;
	else if(strcmp(effort, "6")==0)
		return 6;
	return -1;
}

/* Scratch is assumed big enough to house all working memory encodings, aka
threads * qoip_maxsize(desc)
*/

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, void *tmp, int threads, int entropy) {
	int level = qoip_effortlevel(effort);
	if(level==-1)     /*Custom string*/
		return qoipcrunch_encode_custom(data, desc, out, out_len, effort, tmp, threads, entropy);
	else if(level==0) /*Escape hatch to use best (known, on average) combination*/
		return qoip_encode(data, desc, out, out_len, "a04002e224c482c5a6", entropy, tmp);
	else if(level<4)  /*Stat integers are the best way to search the combination list, level 0..2*/
		return qoipcrunch_encode_smart(data, desc, out, out_len, level-1, tmp, threads, entropy);
	else              /*Search orders of magnitude more combinations with log tables, level 0..2*/
		return qoipcrunch_encode_smarter(data, desc, out, out_len, level-4, tmp, threads, entropy);
	return 1;
}

int qoipcrunch_encode_custom(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, void *tmp, int threads, int entropy) {
	char *next_opstring, *combination_list = (*effort=='t') ? qoipcrunch_test : effort;
	size_t working_cnt;
	if(strchr(combination_list, ',')==NULL)/*Escape hatch for single combinations*/
		return qoip_encode(data, desc, out, out_len, combination_list, entropy, tmp);
	next_opstring = combination_list-1;
	*out_len=-1;
	assert(tmp);
	do {
		++next_opstring;
		if(qoip_encode(data, desc, tmp, &working_cnt, next_opstring, 0, NULL))
			return 1;
		if(*out_len>working_cnt) {/*copy best to out*/
			memcpy(out, tmp, working_cnt);
			*out_len = working_cnt;
		}
	} while( (next_opstring=strchr(next_opstring, ',')) );
	if(entropy)
		qoip_entropy(out, out_len, tmp, entropy);
	return 0;
}

/* Code for smart function follows */

/* Order matters as it determines where in stat integer
an ops flag resides, and ops are ordered in ascending length so that
bultin_ctzll can do some magic to eliminate branching. oplen_table is a lookup table
of op lengths that could possibly be eliminated in favour of some equation
that generates the length, maybe with the help of a sparse stat integer*/
static u8 op_table[] = {OP_LUMA1_232, OP_LUMA1_232B, OP_DELTAA, OP_DIFF1_222, OP_DELTA, OP_INDEX3, OP_INDEX4, OP_INDEX5, OP_INDEX6, OP_INDEX7, OP_INDEX8, OP_A, OP_LUMA2_3433, OP_LUMA2_454, OP_LUMA2_464, OP_LUMA3_4645, OP_LUMA3_5654, OP_LUMA3_676, OP_LUMA3_686, OP_LUMA3_787, OP_LUMA4_7876, /*OP_RGB, OP_RGBA*/};
const static u8 oplen_table[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 5};

typedef struct {
	u64 opmask;
	u8 op[16];
	int op_cnt, best_index, run1, run2;
	size_t best_cnt;
} thread_sim_t;

/* Convert a known-good opstring to a byte array */
static void opstring_to_bytes(char *opstring, u8 *bytes, int *bytes_cnt) {
	*bytes_cnt=0;
	for(*bytes_cnt=0; opstring[2*(*bytes_cnt)]; ++*bytes_cnt)
		bytes[*bytes_cnt] = (qoip_valid_hex(opstring[2*(*bytes_cnt)])<<4) + qoip_valid_hex(opstring[(2*(*bytes_cnt))+1]);
}

/* Calculate how many encoded bytes a run consumes with given run1/run2 sizes */
static inline size_t qoip_sim_run(int run1_len, int run2_len, size_t run) {
	size_t ret = 0;
	for(; run>=run2_len; run-=run2_len)
		ret += 2;
	if(run>run1_len)
		ret += 2;
	else if(run)
		++ret;
	return ret;
}

static inline int find_index(u8 *table, u8 key) {
	int ret=0;
	for(;;++ret) {
		if(table[ret]==key)
			return ret;
	}
	return -1;
}

static size_t run_tot(int run1, int run2, size_t *run_short, size_t *run_long, size_t run_long_cnt) {
	size_t k, ret = 0;
	for(k=0;k<256;++k)/*short runs*/
		ret += run_short[k]*qoip_sim_run(run1, run2, k+1);
	for(k=0;k<run_long_cnt;++k)/*long runs*/
		ret += qoip_sim_run(run1, run2, run_long[k]);
	return ret;
}

static inline void smart_encode_run(qoip_working_t *q, size_t *run_short, size_t **run_long, size_t *run_long_cnt, size_t *run_cap) {
	if(q->run) {
		if(q->run<=256)
			++run_short[q->run-1];
		else {
			if(*run_long_cnt==*run_cap) {
				*run_cap += 1024;
				*run_long = realloc(*run_long, sizeof(size_t)*(*run_cap));
				assert(*run_long);
			}
			*run_long[*run_long_cnt++] = q->run;
		}
		q->run = 0;
	}
}

int qoipcrunch_encode_smart(const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level, void *scratch, int threads, int entropy) {
	int best_index=0, i, indexes_mask[6] = {7, 15, 31, 63, 127, 255}, comb_cnt = 4<<(level<<1), thread_cnt;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	size_t best_cnt=-1, *run_long=NULL, run_cap=0, run_long_cnt=0, run_short[256] = {0}, stat_cnt=0;
	u64 *stat=NULL;
	qoip_rgba_t index3[8]={0}, index4[16]={0}, index5[32]={0}, index6[64]={0}, index7[128]={0}, index8[256]={0};
	qoip_rgba_t *indexes[6] = {index3, index4, index5, index6, index7, index8};
	thread_sim_t tmem[QOIP_MAX_THREADS]={0};

	if ( data == NULL || desc == NULL || out == NULL || out_len == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1 )
		return qoip_ret(1, stderr, "qoip_smart: Bad arguments");

	qoip_init_working_memory(q, data, desc);

	/* Stat pass */
	stat = calloc(desc->width*desc->height, sizeof(u64));
	assert(stat);
	q->px_pos = 0;
	for(q->px_h=0;q->px_h<q->height;++q->px_h) {
		for(q->px_w=0;q->px_w<q->width;++q->px_w) {
			q->px_prev.v = q->px.v;
			if(q->channels==4)
				q->px = *(qoip_rgba_t *)(q->in + q->px_pos);
			else {
				q->px.rgba.r = q->in[q->px_pos + 0];
				q->px.rgba.g = q->in[q->px_pos + 1];
				q->px.rgba.b = q->in[q->px_pos + 2];
			}
			if (q->px.v == q->px_prev.v)
				++q->run;/* Accumulate as much RLE as there is */
			else {
				smart_encode_run(q, run_short, &run_long, &run_long_cnt, &run_cap);
				/* generate variables that may be needed by ops */
				if (q->px_w<8192) {
					q->px_ref.rgba.r = (q->px_prev.rgba.r + q->upcache[(q->px_w * 3) + 0]+1) >> 1;
					q->px_ref.rgba.g = (q->px_prev.rgba.g + q->upcache[(q->px_w * 3) + 1]+1) >> 1;
					q->px_ref.rgba.b = (q->px_prev.rgba.b + q->upcache[(q->px_w * 3) + 2]+1) >> 1;
				}
				else
					q->px_ref.v = q->px_prev.v;
				q->hash = QOIP_COLOR_HASH(q->px);
				q->vr = q->px.rgba.r - q->px_prev.rgba.r;
				q->vg = q->px.rgba.g - q->px_prev.rgba.g;
				q->vb = q->px.rgba.b - q->px_prev.rgba.b;
				q->va = q->px.rgba.a - q->px_prev.rgba.a;
				q->avg_r = q->px.rgba.r - q->px_ref.rgba.r;
				q->avg_g = q->px.rgba.g - q->px_ref.rgba.g;
				q->avg_b = q->px.rgba.b - q->px_ref.rgba.b;
				q->avg_gr = q->avg_r - q->avg_g;
				q->avg_gb = q->avg_b - q->avg_g;
				q->mag_gr = q->avg_gr>=0?q->avg_gr:-(q->avg_gr+1);
				q->mag_g = q->avg_g>=0?q->avg_g:-(q->avg_g+1);
				q->mag_gb = q->avg_gb>=0?q->avg_gb:-(q->avg_gb+1);
				q->mag_rb = q->mag_gr>q->mag_gb?q->mag_gr:q->mag_gb;
				/* Gather stats */
				stat[stat_cnt] = 0;
				/*  LUMA */
				if(q->va) {
					if( q->va > -5 && q->va < 4 &&
						q->mag_rb < 4 &&
						q->mag_g  < 8 )
						stat[stat_cnt] |= ((1<<12)|(3<<15)|(1<<20));//3433 4645 5654 7876
					else if( q->va   > -17 && q->va   < 16 &&
						q->mag_rb <  8 &&
						q->mag_g  < 32 )
						stat[stat_cnt] |= ((3<<15)|(1<<20));//4645 5654 7876
					else if( q->va   >  -9 && q->va    < 8 &&
						q->mag_rb < 16 &&
						q->mag_g  < 32 )
						stat[stat_cnt] |= ((1<<16)|(1<<20));//5654 7876
					else if( q->va   > -33 && q->va   < 32 &&
						q->mag_rb < 64 )
						stat[stat_cnt] |= (1<<20);//7876
				}
				else {
					if( q->mag_rb < 2 && q->mag_g  < 4 )
						stat[stat_cnt] |= (1|(511<<12));//all
					else if( q->mag_rb < 4 && q->mag_g  < 8 )
						stat[stat_cnt] |= (511<<12);//3433+
					else if( q->mag_rb <  8 && q->mag_g  < 16 )
						stat[stat_cnt] |= (255<<13);//454+
					else if( q->mag_rb <  8 && q->mag_g  < 32 )
						stat[stat_cnt] |= (127<<14);//464+
					else if( q->mag_rb < 16 && q->mag_g  < 32 )
						stat[stat_cnt] |= (31<<16);//5654+
					else if( q->mag_rb < 32 && q->mag_g  < 64 )
						stat[stat_cnt] |= (15<<17);//676+
					else if( q->mag_rb < 32 )
						stat[stat_cnt] |= (7<<18);//686 787 7876
					else if( q->mag_rb < 64 )
						stat[stat_cnt] |= (3<<19);//787 7876
				}
				/* Unique ops */
				if(q->va==0) {
					if( q->avg_g > -5 && q->avg_g < 0 &&
						q->avg_gr > -2 && q->avg_gr < 3 &&//cannot use mag
						q->avg_gb > -2 && q->avg_gb < 3 )
						stat[stat_cnt] |= (1 << 1);//232b
					else if( q->avg_g > -1 && q->avg_g < 4 &&
						q->mag_rb < 2 )
						stat[stat_cnt] |= (1 << 1);//232b
					if( q->avg_r > -3 && q->avg_r < 2 &&
						q->mag_g < 2 &&
						q->avg_b > -3 && q->avg_b < 2 )
						stat[stat_cnt] |= (1 << 3);//diff 222
					if( q->avg_r > -2 && q->avg_r < 2 &&
						q->avg_g > -2 && q->avg_g < 2 &&
						q->avg_b > -2 && q->avg_b < 2 )
						stat[stat_cnt] |= (1 << 4);//delta
					stat[stat_cnt] |= (1 << 21);//OP_RGB
				}
				if( q->vr == 0 && q->vg == 0 && q->vb == 0 &&
					q->va > -3 && q->va < 3 )
					stat[stat_cnt] |= (1 << 4);//delta
				if( (q->va == -1 || q->va == 1) &&
					q->avg_r > -2 && q->avg_r < 2 &&
					q->avg_g > -2 && q->avg_g < 2 &&
					q->avg_b > -2 && q->avg_b < 2 )
					stat[stat_cnt] |= (1 << 2);//deltaa
				else if( q->vr == 0 && q->vg == 0 && q->vb == 0 &&
					q->va > -6 && q->va < 5 )
					stat[stat_cnt] |= (1 << 2);//deltaa
				if( q->vr == 0 && q->vg == 0 && q->vb == 0 )
					stat[stat_cnt] |= (1 << 11);//a
				for(i=5;i<11;++i) {/*hash index*/
					if(indexes[i-5][q->hash & indexes_mask[i-5]].v == q->px.v)
						stat[stat_cnt] |= (1 << i);
					indexes[i-5][q->hash & indexes_mask[i-5]] = q->px;
				}
				stat[stat_cnt] |= (1 << 22);//OP_RGBA always present so ctzll has a fallback
				++stat_cnt;
			}
			if(q->px_w<8192) {
				q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
				q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
				q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
			}
			q->px_pos += desc->channels;
		}
	}
	smart_encode_run(q, run_short, &run_long, &run_long_cnt, &run_cap);

	/* Processing pass */
	thread_cnt = threads==0 ? omp_get_num_procs() : threads;
	thread_cnt = thread_cnt>QOIP_MAX_THREADS ? QOIP_MAX_THREADS : thread_cnt;
	thread_cnt = comb_cnt<thread_cnt ? comb_cnt : thread_cnt;
	omp_set_num_threads(thread_cnt);

	#pragma omp parallel
	{/*init working memory*/
		tmem[omp_get_thread_num()].best_cnt=-1;
		tmem[omp_get_thread_num()].best_index=-1;
	}
	#pragma omp parallel for
	for(i=0;i<comb_cnt;++i) {
		size_t k, m, curr=0;
		opstring_to_bytes(qoipcrunch_unified[i], tmem[omp_get_thread_num()].op, &(tmem[omp_get_thread_num()].op_cnt));
		curr+=16;//qoip header
		curr+=16;//bitstream min
		if(tmem[omp_get_thread_num()].op_cnt>6)
			curr+= 8*(((tmem[omp_get_thread_num()].op_cnt-6)/8)+1);
		/* run handling */
		tmem[omp_get_thread_num()].run1=253;/*get run sizes*/
		for(k=0;k<tmem[omp_get_thread_num()].op_cnt;++k)
			tmem[omp_get_thread_num()].run1 -= QOIP_OPCNT(tmem[omp_get_thread_num()].op[k]);
		assert(tmem[omp_get_thread_num()].run1>=0);
		tmem[omp_get_thread_num()].run2=tmem[omp_get_thread_num()].run1 + 256;
		run_tot(tmem[omp_get_thread_num()].run1, tmem[omp_get_thread_num()].run2, run_short, run_long, run_long_cnt);
		/*build op mask*/
		tmem[omp_get_thread_num()].opmask = 3<<21;
		for(m=0;m<tmem[omp_get_thread_num()].op_cnt;++m) {
			tmem[omp_get_thread_num()].opmask |= 1<<find_index(op_table, tmem[omp_get_thread_num()].op[m]);
		}
		/* Use op mask */
		for(k=0;k<stat_cnt;++k)
			curr += oplen_table[__builtin_ctzll(tmem[omp_get_thread_num()].opmask & stat[k])];
		curr += 8;
		if(curr%8)
			curr = ((curr/8)+1)*8;
		if(curr<tmem[omp_get_thread_num()].best_cnt) {
			tmem[omp_get_thread_num()].best_cnt = curr;
			tmem[omp_get_thread_num()].best_index = i;
		}
	}
	/*aggregate OpenMP*/
	for(i=0;i<thread_cnt;++i) {
		if(best_cnt > tmem[i].best_cnt) {
			best_cnt = tmem[i].best_cnt;
			best_index = tmem[i].best_index;
		}
	}
	if(stat)
		free(stat);
	if(run_long)
		free(run_long);

	return qoip_encode(data, desc, out, out_len, qoipcrunch_unified[best_index], entropy, scratch);
}

/* Smarter function that does away with an ordered list of good combinations.
Instead it tries to analyse the stat pass to eliminate ops that poorly fit the data,
then tries many combinations of the remaining ops */

/* Completely redo how combination passes are done
	* For every index1/index2/delta1 combination
		* Get their sizes
		* Create a LUMA log table which counts the log size of mag_rb/mag_g/mag_a
		of each pixel
	* A combination pass then chooses an index1/index2/delta combo and iterates
	the LUMA_RGB2/3 and LUMARGBA2/3/4 ops chosen
	* Calculating size from there is a matter of summing over the small luma_log table
*/

/*Used whenever nop is selected from a set. Always returns false as a nop cannot encode anything*/
int qoip_false(qoip_working_t *q) {
	return 0;
}
int qoip_sim_deltaa(qoip_working_t *q) {
	if (
		(q->va == -1 || q->va == 1) &&
		q->avg_r > -2 && q->avg_r < 2 &&
		q->avg_g > -2 && q->avg_g < 2 &&
		q->avg_b > -2 && q->avg_b < 2
	) {
		return 1;
	}
	else if (/*encode small changes in a, -5..4*/
		q->vr == 0 && q->vg == 0 && q->vb == 0 &&
		q->va > -6 && q->va < 5
	) {
		return 1;
	}
	return 0;
}
int qoip_sim_delta(qoip_working_t *q) {
	if (
		q->va == 0 &&
		q->avg_r > -2 && q->avg_r < 2 &&
		q->avg_g > -2 && q->avg_g < 2 &&
		q->avg_b > -2 && q->avg_b < 2
	) {
		return 1;
	}
	else if (
		q->vr == 0 && q->vg == 0 && q->vb == 0 &&
		q->va > -3 && q->va < 3
	) {
		return 1;
	}
	return 0;
}
int qoip_sim_diff1_222(qoip_working_t *q) {
	if (
		q->va == 0 &&
		q->avg_r > -3 && q->avg_r < 2 &&
		q->avg_g > -3 && q->avg_g < 2 &&
		q->avg_b > -3 && q->avg_b < 2
	) {
		return 1;
	}
	return 0;
}
int qoip_sim_luma1_232b(qoip_working_t *q) {
	if (
		q->va == 0 &&
		q->avg_g   > -5 && q->avg_g   < 0 &&
		q->avg_gr > -2 && q->avg_gr < 3 &&
		q->avg_gb > -2 && q->avg_gb < 3
	) {
		return 1;
	}
	else if (
		q->va == 0 &&
		q->avg_g   > -1 && q->avg_g   < 4 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_gb > -3 && q->avg_gb < 2
	) {
		return 1;
	}
	return 0;
}
static inline int qoip_sim_luma1_222(qoip_working_t *q) {
	if ( q->va==0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -3 && q->avg_g  < 2 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		return 1;
	}
	return 0;
}
static inline int qoip_sim_luma1_232(qoip_working_t *q) {
	if ( q->va==0 &&
		q->avg_gr > -3 && q->avg_gr < 2 &&
		q->avg_g  > -5 && q->avg_g  < 4 &&
		q->avg_gb > -3 && q->avg_gb < 2 ) {
		return 1;
	}
	return 0;
}

typedef struct {
	size_t base_size;//Size of index1/index2/delta1 encodings combined
	size_t lumalog[7*6*9];
} logstat;

/* full log2 of range -128..127: log_lookup[num+128]*/
int log_lookup_0_8[256]={
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,4,4,4,4,3,3,2,1,
	0,2,3,3,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

/* Small logs clamped to a minimum of 2*/
int log_lookup_2_8[256]={
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,4,4,4,4,3,3,2,2,
	2,2,3,3,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

/* Small logs clamped to a minimum of 3*/
int log_lookup_3_8[256]={
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,4,4,4,4,3,3,3,3,
	3,3,3,3,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

/* Packed log range of LUMA ops, 0 is non-LUMA/nop: op_log_lookup[opcode]
Packed as 0xAG(RB), ie 0x234 would indicate OP_LUMA2_4342 if it existed*/
int op_log_lookup[256]={
0,0,0,0x32,0x55,0x453,0x675,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0x64,0x87,0x22,0x353,0x575,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0x54,0x343,0x77,0x565,0x787,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0x86,0x465,0x687,0x44,0x243,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0x76,0x564,0x53,0x342,0x786,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0x43,0x66,0x242,0x464,0x686,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0x33,0x75,0x232,0x454,0x676,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0x42,0x65,0x132,0x354,0x576,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

enum {STATOP_INDEX1_CNT=5, STATOP_RGB1_CNT=5, STATOP_RGBA1_CNT=2, STATOP_INDEX2_CNT=3};
#define STATOP_CNT_MAX (STATOP_INDEX1_CNT*STATOP_RGB1_CNT*STATOP_RGBA1_CNT*STATOP_INDEX2_CNT)
#define LOGSTAT_INDEX_RGBA(a, b, c, d) (((a)*STATOP_INDEX1_CNT*STATOP_RGB1_CNT*STATOP_INDEX2_CNT)+((b)*STATOP_RGB1_CNT*STATOP_INDEX2_CNT)+((c)*STATOP_INDEX2_CNT)+(d))
#define LOGSTAT_INDEX_RGB(b, c, d)                                                               (((b)*STATOP_RGB1_CNT*STATOP_INDEX2_CNT)+((c)*STATOP_INDEX2_CNT)+(d))
#define LUMALOG_INDEX_RGBA(a, b, c) (((a)*42)+(((b)-3)*7)+((c)-2))
#define LUMALOG_INDEX_RGB(b, c)              ((((b)-3)*7)+((c)-2))

/*Functions to deduplicate code*/
static inline void smarter_gen_common(qoip_working_t *q, int *log_g, int *log_rb) {
	int log_r, log_b;
	if (q->px_w<8192) {
		q->px_ref.rgba.r = (q->px_prev.rgba.r + q->upcache[(q->px_w * 3) + 0]+1) >> 1;
		q->px_ref.rgba.g = (q->px_prev.rgba.g + q->upcache[(q->px_w * 3) + 1]+1) >> 1;
		q->px_ref.rgba.b = (q->px_prev.rgba.b + q->upcache[(q->px_w * 3) + 2]+1) >> 1;
	}
	else
		q->px_ref.v = q->px_prev.v;
	q->hash = QOIP_COLOR_HASH(q->px);
	q->vr = q->px.rgba.r - q->px_prev.rgba.r;
	q->vg = q->px.rgba.g - q->px_prev.rgba.g;
	q->vb = q->px.rgba.b - q->px_prev.rgba.b;
	q->avg_r = q->px.rgba.r - q->px_ref.rgba.r;
	q->avg_g = q->px.rgba.g - q->px_ref.rgba.g;
	q->avg_b = q->px.rgba.b - q->px_ref.rgba.b;
	q->avg_gr = q->avg_r - q->avg_g;
	q->avg_gb = q->avg_b - q->avg_g;
	log_r  = log_lookup_2_8[q->avg_gr + 128];
	*log_g  = log_lookup_3_8[q->avg_g  + 128];
	log_b  = log_lookup_2_8[q->avg_gb + 128];
	*log_rb = log_r>log_b?log_r:log_b;
}

int qoipcrunch_encode_smarter(const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level, void *scratch, int threads, int entropy) {
	int isrgb=0, use_a=0;
	size_t *run_long=NULL, run_cap=0, run_long_cnt=0, run_short[256] = {0}, run_lookup[256];

	size_t i, j, comb, comb_cnt, explicit_cnt, best_cnt=-1, curr_cnt;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	/*index1/2 constants*/
	qoip_rgba_t index3[8]={0}, index4[16]={0}, index5[32]={0}, index6[64]={0}, index7[128]={0}, index8[256]={0}, index9[512]={0}, index10[1024]={0};
	qoip_rgba_t *indexes1[5] = {index6, index7, index5, index4, index3}, *indexes2[3] = {index8, index9, index10};
	const u8 statop_index2[] = {OP_INDEX8, OP_INDEX9, OP_INDEX10};
	const u8 statop_index1[] = {OP_INDEX6, OP_INDEX7, OP_INDEX5, OP_INDEX4, OP_INDEX3};
	const int index1_mask[5] = {63, 127, 31, 15, 7}, index2_mask[3] = {255, 511, 1023};
	/*rgb1 constants*/
	const u8 statop_rgb1[] = {OP_LUMA1_232B, OP_DIFF1_222, OP_DELTA, OP_LUMA1_232, OP_LUMA1_222};
	int (*sim_delta1[]) (qoip_working_t *) = {qoip_sim_luma1_232b, qoip_sim_diff1_222, qoip_sim_delta, qoip_sim_luma1_232, qoip_sim_luma1_222};
	int res_delta1[5];

	const u8 statop_rgb2[] = {OP_LUMA2_464,   OP_LUMA2_454,  OP_LUMA2_555,  OP_LUMA2_444,  OP_LUMA2_353,  OP_LUMA2_343,  OP_LUMA2_333, OP_LUMA2_242};
	const u8 statop_rgb3[] = {OP_LUMA3_787, OP_LUMA3_686, OP_LUMA3_676,  OP_LUMA3_575,  OP_LUMA3_565,  OP_LUMA3_666,  OP_LUMA3_777};

	const u8 statop_rgba1[] = {255, OP_DELTAA};
	int (*sim_delta2[]) (qoip_working_t *) = {qoip_false, qoip_sim_deltaa};
	int res_delta2[2] = {0};

	const u8 statop_rgba2[] = {255, OP_LUMA2_3433, OP_LUMA2_3533, OP_LUMA2_3534, OP_LUMA2_2322, OP_LUMA2_2422, OP_LUMA2_2423, OP_LUMA2_3432};
	const u8 statop_rgba3[] = {255, OP_LUMA3_4543, OP_LUMA3_4544, OP_LUMA3_4644, OP_LUMA3_4645, OP_LUMA3_5654, OP_LUMA3_5655, OP_LUMA3_5755, OP_LUMA3_5756};
	const u8 statop_rgba4[] = {255, OP_LUMA4_7876, OP_LUMA4_6766, OP_LUMA4_6866, OP_LUMA4_6765, OP_LUMA4_6867, OP_LUMA4_7877};
	int rgb_cnts[] = {
		4, 4,    3, 4,    3,/*level 0*/
		5, 4,    3, 6,    5,/*level 1*/
		5, 5,    3, 8,    7,/*level 2*/
	};
	int rgba_cnts[] = {
		4, 4, 2, 3, 4, 4, 3, 4, 4,/*level 0*/
		5, 4, 2, 3, 6, 5, 5, 6, 6,/*level 1*/
		5, 5, 2, 3, 8, 8, 7, 9, 7,/*level 2*/
	};

	/*statop sets in the order they should be tested*/
	const u8* statops_rgb[] = {statop_index1, statop_rgb1, statop_index2, statop_rgb2, statop_rgb3};
	const u8* statops_rgba[] = {statop_index1, statop_rgb1, statop_rgba1, statop_index2, statop_rgb2, statop_rgba2, statop_rgb3, statop_rgba3, statop_rgba4};
	const int statops_rgb_cnt = 5, statops_rgba_cnt = 9;
	const int rgb_lengths[] = {1, 1, 2, 2, 3};
	const int rgba_lengths[] = {1, 1, 1, 2, 2, 2, 3, 3, 4};
	u8 choice[12], best_choice[12]={0};
	char opstr[32]={0};
	/* sets* populated with rgb/rgba depending on input */
	const u8 **sets;
	int sets_cnt;
	const int *set_cnts;
	const int *set_lengths;

	int log_g, log_rb, log_a, lumalog_loc;
	logstat *log, log_configs[STATOP_CNT_MAX] = {0};

	int it_index1, it_index2, it_delta1, it_delta2;

	if ( data == NULL || desc == NULL || out == NULL || out_len == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1 )
		return qoip_ret(1, stderr, "qoip_smarter: Bad arguments");

	qoip_init_working_memory(q, data, desc);
	/* Stat pass */
	q->px_pos = 0;
	if(q->channels==3) {
		isrgb=1;
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px.rgba.r = q->in[q->px_pos + 0];
				q->px.rgba.g = q->in[q->px_pos + 1];
				q->px.rgba.b = q->in[q->px_pos + 2];
				if (q->px.v == q->px_prev.v)
					++q->run;
				else {
					smart_encode_run(q, run_short, &run_long, &run_long_cnt, &run_cap);
					smarter_gen_common(q, &log_g, &log_rb);
					lumalog_loc = LUMALOG_INDEX_RGB(log_g, log_rb);
					for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1)
						res_delta1[it_delta1] = sim_delta1[it_delta1](q);
					for(it_index1=0;it_index1<rgba_cnts[(level*9)+0];++it_index1) {
						if(indexes1[it_index1][q->hash & index1_mask[it_index1]].v == q->px.v) {
							for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1) {
								for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
									log_configs[LOGSTAT_INDEX_RGB(it_index1, it_delta1, it_index2)].base_size++;
								}
							}
						}
						else {///*Not handled by index1 op
							for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1) {
								if(res_delta1[it_delta1]) {
									for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
										log_configs[LOGSTAT_INDEX_RGB(it_index1, it_delta1, it_index2)].base_size++;
									}
								}
								else {///*Not handled by delta1 op
									for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
										if(indexes2[it_index2][q->hash & index2_mask[it_index2]].v == q->px.v)
											log_configs[LOGSTAT_INDEX_RGB(it_index1, it_delta1, it_index2)].base_size+=2;
										else///*Not handled by index2 op, add to luma_log
											log_configs[LOGSTAT_INDEX_RGB(it_index1, it_delta1, it_index2)].lumalog[lumalog_loc]++;
									}
								}
							}
						}
						indexes1[it_index1][q->hash & index1_mask[it_index1]] = q->px;
					}
					for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2)
						indexes2[it_index2][q->hash & index2_mask[it_index2]] = q->px;
				}
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
				q->px_pos += 3;
			}
		}
	}
	else {//RGBA
		for(q->px_h=0;q->px_h<q->height;++q->px_h) {
			for(q->px_w=0;q->px_w<q->width;++q->px_w) {
				q->px_prev.v = q->px.v;
				q->px = *(qoip_rgba_t *)(q->in + q->px_pos);
				if (q->px.v == q->px_prev.v)
					++q->run;
				else {
					smart_encode_run(q, run_short, &run_long, &run_long_cnt, &run_cap);
					smarter_gen_common(q, &log_g, &log_rb);
					q->va = q->px.rgba.a - q->px_prev.rgba.a;
					log_a  = log_lookup_0_8[q->va     + 128];
					lumalog_loc = LUMALOG_INDEX_RGBA(log_a, log_g, log_rb);
					for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1)
						res_delta1[it_delta1] = sim_delta1[it_delta1](q);
					res_delta2[1] = sim_delta2[1](q);
					for(it_index1=0;it_index1<rgba_cnts[(level*9)+0];++it_index1) {
						if(indexes1[it_index1][q->hash & index1_mask[it_index1]].v == q->px.v) {
							for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1) {
								for(it_delta2=0;it_delta2<rgba_cnts[(level*9)+2];++it_delta2) {
									for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
										log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].base_size++;
									}
								}
							}
						}
						else {///*Not handled by index1 op
							for(it_delta1=0;it_delta1<rgba_cnts[(level*9)+1];++it_delta1) {
								if(res_delta1[it_delta1]) {
									for(it_delta2=0;it_delta2<rgba_cnts[(level*9)+2];++it_delta2) {
										for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
											log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].base_size++;
										}
									}
								}
								else {///*Not handled by delta1 op
									for(it_delta2=0;it_delta2<rgba_cnts[(level*9)+2];++it_delta2) {
										if(res_delta2[it_delta2]) {
											for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
												log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].base_size++;
											}
										}
										else {///*Not handled by delta2 op (deltaa)
											for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2) {
												if(indexes2[it_index2][q->hash & index2_mask[it_index2]].v == q->px.v) {
													log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].base_size+=2;
												}
												else {///*Not handled by index2 op, add to luma_log
													if(q->vr==0&&q->vg==0&&q->vb==0) {//OP_A
														use_a=1;
														log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].base_size+=2;
													}
													else
														log_configs[LOGSTAT_INDEX_RGBA(it_delta2, it_index1, it_delta1, it_index2)].lumalog[lumalog_loc]++;
												}
											}
										}
									}
								}
							}
						}
						indexes1[it_index1][q->hash & index1_mask[it_index1]] = q->px;
					}
					for(it_index2=0;it_index2<rgba_cnts[(level*9)+3];++it_index2)
						indexes2[it_index2][q->hash & index2_mask[it_index2]] = q->px;
				}
				if(q->px_w<8192) {
					q->upcache[(q->px_w * 3) + 0] = q->px.rgba.r;
					q->upcache[(q->px_w * 3) + 1] = q->px.rgba.g;
					q->upcache[(q->px_w * 3) + 2] = q->px.rgba.b;
				}
				q->px_pos += 4;
			}
		}
	}
	smart_encode_run(q, run_short, &run_long, &run_long_cnt, &run_cap);// Cap last run

	/*Determine if 4 channel input is RGB or RGBA*/
	if(!isrgb) {
		size_t sc, gg, rr, aa;
		isrgb=1;
		for(sc=0;sc<STATOP_CNT_MAX;++sc) {
			for(gg=3;gg<9;++gg){
				for(rr=2;rr<9;++rr){
					for(aa=1;aa<9;++aa){
						if(log_configs[sc].lumalog[LUMALOG_INDEX_RGBA(aa, gg, rr)]) {
							isrgb=0;
							goto stageleft;
						}
					}
				}
			}
		}
	}
	stageleft:

	/* Process run data into lookup table to avoid redoing work */
	for(i=0;i<256;++i)
		run_lookup[i] = run_tot(i, i+256, run_short, run_long, run_long_cnt);

	/* Processing pass */
	if(isrgb) {
		sets = statops_rgb;
		sets_cnt = statops_rgb_cnt;
		set_cnts = rgb_cnts+(level*sets_cnt);
		set_lengths = rgb_lengths;
		comb_cnt = 1;
		for(i=0;i<sets_cnt;++i)
			comb_cnt *= set_cnts[i];
		for(i=0;i<comb_cnt;++i) {
			int cindex[12];/*index of the current choice*/
			/*Choose ops from sets*/
			comb=i;
			for(j=0;j<sets_cnt;++j) {
				cindex[j]=comb%set_cnts[j];
				choice[j] = sets[j][comb%set_cnts[j]];
				comb /= set_cnts[j];
			}
			/*Find explicit length*/
			explicit_cnt = 0;
			for(j=0;j<sets_cnt;++j)
				explicit_cnt += QOIP_OPCNT(choice[j]);
			if(explicit_cnt>253)
				continue;

			/*Test combination*/
			log = log_configs + LOGSTAT_INDEX_RGB(cindex[0], cindex[1], cindex[2]);
			curr_cnt = run_lookup[256-(explicit_cnt+3)] + log->base_size;
			int g, r, oplog[12];
			for(j=3;j<sets_cnt;++j)
				oplog[j] = op_log_lookup[choice[j]];
			for(g=3;g<=8;++g) {
				for(r=2;r<=8;++r) {
					for(j=3;j<sets_cnt;++j) {
						assert(oplog!=0);
						if( r<=(oplog[j]&15) && g<=((oplog[j]>>4)&15) ) {
							curr_cnt += (set_lengths[j]*log->lumalog[LUMALOG_INDEX_RGB(g, r)]);
							break;
						}
					}
					if(j==sets_cnt)
						curr_cnt += (4*log->lumalog[LUMALOG_INDEX_RGB(g, r)]);
				}
			}

			if(best_cnt>curr_cnt) {
				best_cnt=curr_cnt;
				for(j=0;j<sets_cnt;++j)
					best_choice[j]=choice[j];
			}
		}
	}
	else {//RGBA
		sets = statops_rgba;
		sets_cnt = statops_rgba_cnt;
		set_cnts = rgba_cnts+(level*sets_cnt);
		set_lengths = rgba_lengths;
		comb_cnt = 1;
		for(i=0;i<sets_cnt;++i)
			comb_cnt *= set_cnts[i];
		for(i=0;i<comb_cnt;++i) {
			int cindex[12];/*index of the current choice*/
			/*Choose ops from sets*/
			comb=i;
			for(j=0;j<sets_cnt;++j) {
				cindex[j]=comb%set_cnts[j];
				choice[j] = sets[j][comb%set_cnts[j]];
				comb /= set_cnts[j];
			}
			/*Find explicit length*/
			explicit_cnt = use_a;
			for(j=0;j<sets_cnt;++j) {
				if(choice[j]!=255)
					explicit_cnt += QOIP_OPCNT(choice[j]);
			}
			if(explicit_cnt<192 || explicit_cnt>253)
				continue;

			/*Test combination*/
			log = log_configs + LOGSTAT_INDEX_RGBA(cindex[2], cindex[0], cindex[1], cindex[3]);
			curr_cnt = run_lookup[256-(explicit_cnt+3)] + log->base_size;
			int a, g, r, oplog[12];
			for(j=4;j<sets_cnt;++j)
				oplog[j] = op_log_lookup[choice[j]];
			for(a=0;a<=8;++a) {
				for(g=3;g<=8;++g) {
					for(r=2;r<=8;++r) {
						for(j=4;j<sets_cnt;++j) {
							assert(oplog!=0);
							if( r<=(oplog[j]&15) && g<=((oplog[j]>>4)&15) && a<=((oplog[j]>>8)&15) ) {
								curr_cnt += (set_lengths[j]*log->lumalog[LUMALOG_INDEX_RGBA(a, g, r)]);
								break;
							}
						}
						if(j==sets_cnt)
							curr_cnt += ((a==0?4:5)*log->lumalog[LUMALOG_INDEX_RGBA(a, g, r)]);
					}
				}
			}
			if(best_cnt>curr_cnt) {
				best_cnt=curr_cnt;
				for(j=0;j<sets_cnt;++j)
					best_choice[j]=choice[j];
			}
		}
	}

	{/*Build best opstring*/
		int strloc=0;
		for(j=0;j<sets_cnt;++j) {
			if(best_choice[j]!=255) {
				sprintf(opstr+strloc, "%02x", best_choice[j]);
				strloc+=2;
			}
		}
		if(use_a)
			sprintf(opstr+strloc, "%02x", OP_A);
	}

	if(run_long)
		free(run_long);

	return qoip_encode(data, desc, out, out_len, opstr, entropy, scratch);
}

#endif /* QOIPCRUNCH_C */
