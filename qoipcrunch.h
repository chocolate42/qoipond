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

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *scratch, int threads, int entropy);
int qoipcrunch_encode_smart(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *scratch, int threads, int entropy);

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

static void qoipcrunch_update_stats(size_t *currbest_len, char *currbest_str, size_t *candidate_len, char *candidate_str) {
	size_t len;
	if(*candidate_len<*currbest_len) {
		//printf("New best length  %8"PRIu64" with opstring %s\n", *candidate_len, candidate_str?candidate_str:"[default]");
		if(candidate_str) {
			len = strchr(candidate_str, ',') ? strchr(candidate_str, ',')-candidate_str : strlen(candidate_str);
			memcpy(currbest_str, candidate_str, len);
			currbest_str[len] = 0;
		}
		else
			currbest_str="";
		*currbest_len=*candidate_len;
	}
}

/* Working memory for threads */
typedef struct {
	u8 *curr;/*Encoding out*/
	int best/*best index tested*/, curr_index;/*Current index*/
	size_t best_len/*Length of best tested*/, curr_len;/*Current length*/
} tm_t;

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
int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *tmp, int threads, int entropy) {
	char *combination_list, currbest_str[256], *next_opstring;
	int currbest = -1, j;
	size_t currbest_len=-1, w_len;
	size_t cnt = 0;
	int level = -1;
	u8 *scratch = (u8*)tmp;
	void *working = scratch?scratch:out;
	size_t *working_len = scratch?&w_len:out_len;
	int list_cnt, thread_cnt;
	tm_t tm[QOIP_MAX_THREADS] = {0};

	level = qoip_effortlevel(effort);

	assert(scratch);//why?

	if(level==-1) {/* Try every combination in the user-defined list */
		combination_list = (*effort=='t') ? qoipcrunch_test : effort;
		if(strchr(combination_list, ',')==NULL)/*Escape hatch for single-combination runs to avoid memcpy*/
			return qoip_encode(data, desc, out, out_len, combination_list, entropy, tmp);
		next_opstring = combination_list-1;
		do {
			++next_opstring;
			if(qoip_encode(data, desc, working, working_len, next_opstring, 0, NULL))
				return 1;
			if(currbest_len>*working_len) {/*copy best to out*/
				memcpy(out, working, *working_len);
				*out_len = *working_len;
			}
			++cnt;
			qoipcrunch_update_stats(&currbest_len, currbest_str, working_len, next_opstring);
		} while( (next_opstring=strchr(next_opstring, ',')) );
		if(count)
			*count=cnt;
		if(entropy)
			qoip_entropy(out, out_len, tmp, entropy);
		return 0;
	}
	else if(level==0)/*escape hatch for effort level 0 which can directly encode*/
		return qoip_encode(data, desc, out, out_len, qoipcrunch_unified[0], entropy, tmp);

	list_cnt = 1 << level;

	/*Dynamic threads currently disabled by arg parsing as it requires scratch
	to be dynamically allocated, which we are not doing. Fix or remove TODO */
	thread_cnt = threads==0 ? omp_get_num_procs() : threads;
	thread_cnt = thread_cnt>QOIP_MAX_THREADS ? QOIP_MAX_THREADS : thread_cnt;
	thread_cnt = list_cnt<thread_cnt ? list_cnt : thread_cnt;
	omp_set_num_threads(thread_cnt);

	#pragma omp parallel
	{/*init working memory*/
		tm[omp_get_thread_num()].best = -1;
		tm[omp_get_thread_num()].best_len = currbest_len;
		tm[omp_get_thread_num()].curr = scratch + (omp_get_thread_num()*currbest_len);
	}

	#pragma omp parallel for
	for(j=0;j<list_cnt;++j) {/*try combinations*/
		qoip_encode(data, desc, tm[omp_get_thread_num()].curr, &tm[omp_get_thread_num()].curr_len, qoipcrunch_unified[j], 0, NULL);
		tm[omp_get_thread_num()].curr_index=j;
		if(tm[omp_get_thread_num()].best_len > tm[omp_get_thread_num()].curr_len) {
			tm[omp_get_thread_num()].best_len = tm[omp_get_thread_num()].curr_len;
			tm[omp_get_thread_num()].best = j;
		}
	}

	for(j=0;j<thread_cnt;++j) {/*aggregate*/
		if(tm[j].best_len < currbest_len) {
			currbest_len = tm[j].best_len;
			currbest = tm[j].best;
		}
	}
	if(currbest==-1)
		return 1;

	/*Copy encoding to output*/
	for(j=0;j<thread_cnt;++j) {/*If it's in working memory copy it*/
		if(tm[j].curr_index == currbest) {
			memcpy(out, tm[j].curr, tm[j].curr_len);
			*out_len = tm[j].curr_len;
			break;
		}
	}
	if(j == thread_cnt)/*Otherwise redo encode*/
		qoip_encode(data, desc, out, out_len, qoipcrunch_unified[currbest], 0, NULL);

	if(count)
		*count=list_cnt;

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

int qoipcrunch_encode_smart(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *scratch, int threads, int entropy) {
	int best_index=0, i, indexes_mask[6] = {7, 15, 31, 63, 127, 255}, level, thread_cnt;
	qoip_working_t qq = {0};
	qoip_working_t *q = &qq;
	size_t best_cnt=-1, *run=NULL, run_cap=0, run_cnt=0, run_short[256] = {0}, stat_cnt=0;
	u64 *stat=NULL;
	qoip_rgba_t index3[8]={0}, index4[16]={0}, index5[32]={0}, index6[64]={0}, index7[128]={0}, index8[256]={0};
	qoip_rgba_t *indexes[6] = {index3, index4, index5, index6, index7, index8};
	thread_sim_t tmem[QOIP_MAX_THREADS]={0};

	if ( data == NULL || desc == NULL || out == NULL || out_len == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 || desc->colorspace > 1 )
		return qoip_ret(1, stderr, "qoip_smart: Bad arguments");

	level = qoip_effortlevel(effort);
	if(level==-1)
		level=3;
	else if(level==0)
		return qoip_encode(data, desc, out, out_len, qoipcrunch_unified[0], entropy, scratch);

	q->in = (const unsigned char *)data;
	q->px.v = 0;
	q->px.rgba.a = 255;
	q->width = desc->width;
	q->height = desc->height;
	q->channels = desc->channels;
	q->stride = desc->width * desc->channels;
	q->upcache[0]=0;
	q->upcache[1]=0;
	q->upcache[2]=0;
	for(i=0;i<(desc->width<8192?desc->width-1:8191);++i) {/* Prefill upcache */
		q->upcache[((i+1)*3)+0]=q->in[(i*desc->channels)+0];
		q->upcache[((i+1)*3)+1]=q->in[(i*desc->channels)+1];
		q->upcache[((i+1)*3)+2]=q->in[(i*desc->channels)+2];
	}

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
				if(q->run) {
					if(q->run<=256)
						++run_short[q->run-1];
					else {
						if(run_cnt==run_cap) {
							run_cap += 1024;
							run = realloc(run, sizeof(size_t)*run_cap);
							assert(run);
						}
						run[run_cnt++] = q->run;
					}
					q->run = 0;
				}
				/* generate variables that may be needed by ops */
				if (q->px_w<8192) {
					q->px_ref.rgba.r = (q->px_prev.rgba.r + q->upcache[(q->px_w * 3) + 0]+1) >> 1;
					q->px_ref.rgba.g = (q->px_prev.rgba.g + q->upcache[(q->px_w * 3) + 1]+1) >> 1;
					q->px_ref.rgba.b = (q->px_prev.rgba.b + q->upcache[(q->px_w * 3) + 2]+1) >> 1;
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
	if(q->run) {/* Cap off ending run if present*/
		if(q->run<=256)
			++run_short[q->run-1];
		else {
			if(run_cnt==run_cap) {
				run_cap += 1024;
				run = realloc(run, sizeof(size_t)*run_cap);
				assert(run);
			}
			run[run_cnt++] = q->run;
		}
		q->run = 0;
	}

	/* Processing pass */
	thread_cnt = threads==0 ? omp_get_num_procs() : threads;
	thread_cnt = thread_cnt>QOIP_MAX_THREADS ? QOIP_MAX_THREADS : thread_cnt;
	thread_cnt = (1<<level)<thread_cnt ? (1<<level) : thread_cnt;
	omp_set_num_threads(thread_cnt);

	#pragma omp parallel
	{/*init working memory*/
		tmem[omp_get_thread_num()].best_cnt=-1;
		tmem[omp_get_thread_num()].best_index=-1;
	}
	#pragma omp parallel for
	for(i=0;i<(1<<level);++i) {
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
		for(k=0;k<256;++k)/*short runs*/
			curr += run_short[k]*qoip_sim_run(tmem[omp_get_thread_num()].run1, tmem[omp_get_thread_num()].run2, k+1);
		for(k=0;k<run_cnt;++k)/*long runs*/
			curr += qoip_sim_run(tmem[omp_get_thread_num()].run1, tmem[omp_get_thread_num()].run2, run[k]);
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
	if(run)
		free(run);

	return qoip_encode(data, desc, out, out_len, qoipcrunch_unified[best_index], entropy, scratch);
}

#endif /* QOIPCRUNCH_C */
