/* qoipcrunch.h - Encode wrapper for qoip_encode to search for better combinations

-- LICENSE: The MIT License(MIT)

Copyright(c) 2021 Matthew Ling

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

/* Scratch is assumed big enough to house all working memory encodings, aka
threads * qoip_maxsize(desc)
*/
int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *tmp, int threads, int entropy) {
	char currbest_str[256], *next_opstring;
	int currbest = -1, j;
	size_t currbest_len, w_len;
	size_t cnt = 0;
	int level = -1;
	u8 *scratch = (u8*)tmp;
	void *working = scratch?scratch:out;
	size_t *working_len = scratch?&w_len:out_len;
	int list_cnt, thread_cnt;
	tm_t tm[QOIP_MAX_THREADS] = {0};

	currbest_len = qoip_maxsize(desc);

	if(     strcmp(effort, "0")==0 || !effort)
		level=0;
	else if(strcmp(effort, "1")==0)
		level=1;
	else if(strcmp(effort, "2")==0)
		level=2;
	else if(strcmp(effort, "3")==0)
		level=3;
	else if(strcmp(effort, "4")==0)
		level=4;
	else if(strcmp(effort, "5")==0)
		level=5;
	else if(strcmp(effort, "6")==0)
		level=6;

	assert(scratch);

	if(level==-1) {/* Try every combination in the user-defined list */
		next_opstring = effort-1;
		do {
			++next_opstring;
			if(qoip_encode(data, desc, working, working_len, next_opstring, 0, NULL))
				return 1;
			if(currbest_len>*working_len) {/*copy best to out*/
				memcpy(out, scratch, *working_len);
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

#endif /* QOIPCRUNCH_C */
