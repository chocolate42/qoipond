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
#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level, size_t *count);

#ifdef __cplusplus
}
#endif
#endif /* QOIPCRUNCH_H */

#ifdef QOIPCRUNCH_C
#include <stdio.h>
#include <string.h>

typedef struct {
	int level[3];/*The number of ops to test from the set for a given effort level */
	int ops[8];/* The ops in the set */
	int codespace[8];/* How many opcodes each op uses */
	int hasAlpha[8];/* Does the op deal with alpha */
} qoip_set_t;

static void qoipcrunch_update_stats(size_t *currbest_len, char *currbest_str, size_t *candidate_len, char *candidate_str) {
	if(*candidate_len<*currbest_len) {
		//printf("New best length  %8"PRIu64" with opstring %s\n", *candidate_len, candidate_str?candidate_str:"[default]");
		if(candidate_str)
			strcpy(currbest_str, candidate_str);
		else
			currbest_str="";
		*currbest_len=*candidate_len;
	}
}

/* Go the the next main opcode configuration */
static int qoipcrunch_iterate(int level, qoip_set_t *set, int set_cnt, int *index) {
	int curr = set_cnt-1;
	while(curr>=0) {
		index[curr] = (index[curr]+1)%set[curr].level[level];
		if(index[curr])
			break;
		--curr;
	}
	return curr==-1?1:0;
}

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, int level, size_t *count) {
	char currbest_str[256], opstring[256], opstring2[256];
	int i[5]={0}, set_cnt=5, j, opcnt, opstring_loc, opstring2_loc;
	size_t currbest_len;
	size_t cnt = 0;
	/* Sets of ops where one op must be chosen from each set
	OP_END indicates that "no op" is a valid choice from a set
	These sets are non-overlapping */
	qoip_set_t set[] = {
		{
			{1,4,8},
			{OP_RUN1_4, OP_RUN1_3, OP_RUN1_5, OP_RUN1_6, OP_RUN1_7, OP_RUN1_2, OP_RUN1_1, OP_RUN1_0},
			{16, 8, 32, 64, 128, 4, 2, 1},
		},
		{
			{3,4,7},
			{OP_END, OP_INDEX5, OP_INDEX6, OP_INDEX7, OP_INDEX4, OP_INDEX3, OP_INDEX2},
			{0, 32, 64, 128, 16, 8, 4},
		},
		{ {2,3,3}, {OP_END, OP_DIFF, OP_LUMA1_232}, {0, 64, 128} },
		{ {2,2,2}, {OP_END, OP_LUMA2_464}, {0, 64} },
		{ {2,3,4}, {OP_END, OP_RGB3, OP_LUMA3_676, OP_LUMA3_4645}, {0, 64, 8, 8}, {0, 0, 0, 1} },
	};

	/* 8 bit tags that can be toggled present or not. These tags may overlap RUN1
	encoding as long as at least 1 op remains for RLE. OP_RGB and OP_RGBA not included
	as they are implicit */
	int standalone[] = {OP_A, OP_RUN2};
	int standalone_alpha[] = {1, 0};
	int standalone_cnt = 2, standalone_use;
	u64 standalone_mask;
	char *common[] = {
		"",/*Whatever the default currently is */
		"000102060e121314", /* PropA + OP_A */
		"0001050d1213",     /* QOI */
		"00010203070e13151617",    /* delta9h */
		/* demo28 TODO */
	};
	int common_alpha[] = {0, 1, 0, 1};
	int common_cnt = 4;

	currbest_len = qoip_maxsize(desc);

	if(level==-1) {
		if(qoip_encode(data, desc, out, out_len, NULL))
			return 1;
		++cnt;
		if(count)
			*count=cnt;
		return 0;
	}
	else if(level==0) {
		for(j=0;j<common_cnt;++j) {
			if(qoip_encode(data, desc, out, out_len, common[j]))
				return 1;
			qoipcrunch_update_stats(&currbest_len, currbest_str, out_len, common[j]);
			++cnt;
		}
	}
	else {
		/* Also do level 0 here to make level>0 a superset of level 0 */
		for(j=0;j<common_cnt;++j) {
			if(qoip_encode(data, desc, out, out_len, common[j]))
				return 1;
			qoipcrunch_update_stats(&currbest_len, currbest_str, out_len, common[j]);
			++cnt;
		}
		do {
			opcnt=0;
			for(j=0;j<set_cnt;++j)
				opcnt+=set[j].codespace[i[j]];
			if(opcnt>=240 && opcnt<=256 && ((opcnt-254)<set[0].codespace[i[0]])) {
				opstring_loc=sprintf(opstring, "0001");
				for(j=0;j<set_cnt;++j) {
					if(set[j].ops[i[j]]!=OP_END)
						opstring_loc+=sprintf(opstring+opstring_loc, "%02x", set[j].ops[i[j]]);
				}
				standalone_use=0;
				for(standalone_mask=0; standalone_mask < (1<<standalone_cnt); ++standalone_mask ) {
					strcpy(opstring2, opstring);
					opstring2_loc=opstring_loc;
					for(j=0;j<standalone_cnt;++j) {
						if(standalone_mask & (1<<j)) {
							opstring2_loc+=sprintf(opstring2+opstring2_loc, "%02x", standalone[j]);
							++standalone_use;
						}
					}
					if( ((standalone_use + opcnt - 254)<set[0].codespace[i[0]]) ) {
						if(qoip_encode(data, desc, out, out_len, opstring2))
							return 1;
						++cnt;
					}
				}
				qoipcrunch_update_stats(&currbest_len, currbest_str, out_len, opstring2);
			}
		} while(!qoipcrunch_iterate(level-1, set, set_cnt, i));
	}
	if(count)
		*count=cnt;

	if(*out_len!=currbest_len)
		qoip_encode(data, desc, out, out_len, currbest_str);
	return 0;
}

#endif /* QOIPCRUNCH_C */
