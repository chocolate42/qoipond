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

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *tmp);

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
} qoip_set_t;

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

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *tmp) {
	char currbest_str[256], opstring[256], opstring2[256], *next_opstring;
	int i[8]={0}, j, opcnt, opstring_loc, opstring2_loc;
	size_t currbest_len, w_len;
	size_t cnt = 0;
	int level = -1;
	int standalone_use;
	int standalone_mask;
	int isrgb = desc->channels==3 ? 1 : 0;
	int set_cnt = isrgb ? 4 : 5;/* avoid alpha ops */
	int standalone_cnt = isrgb ? 0 : 1;/* avoid alpha ops */
	void *working = tmp?tmp:out;
	size_t *working_len = tmp?&w_len:out_len;

	/* Sets of ops where one op must be chosen from each set
	OP_END indicates that "no op" is a valid choice from a set
	These sets are non-overlapping */
	qoip_set_t set[] = {
		{
			{2,4,5},
			{OP_INDEX5, OP_INDEX6, OP_INDEX4, OP_INDEX7, OP_END, OP_INDEX3, OP_INDEX2},
			{32, 64, 16, 128, 0, 8, 4},
		},
		{ {1,2,3}, {OP_DIFF, OP_LUMA1_232, OP_DELTA}, {64, 128, 32} },
		{ {1,2,2}, {OP_LUMA2_464, OP_END}, {64, 0} },
		{ {2,2,3}, {OP_END, OP_RGB3, OP_LUMA3_676}, {0, 64, 8} },
		{ {1,2,2}, {OP_END, OP_LUMA3_4645}, {0, 8} },/* alpha */
	};

	/* 8 bit tags that can be toggled present or not. These tags may overlap RUN1
	encoding as long as at least 1 op remains for RLE. OP_RGB and OP_RGBA not included
	as they are implicit, as is OP_RUN2. Alpha tags at end for easy RGB path */
	int standalone[] = {OP_A};

	/* Handpicked combinations for level 0, ideally these would all have fastpaths */
	char *common[] = {
		"0104090a0b0c",/*deltax*/
		"0001020a0e",/*idelta*/
		/* demo28 TODO */
	};
	int common_cnt = 2;

	currbest_len = qoip_maxsize(desc);

	if(!effort)
		effort="";
	if(strcmp(effort, "0")==0)
		level=0;
	else if(strcmp(effort, "1")==0)
		level=1;
	else if(strcmp(effort, "2")==0)
		level=2;
	else if(strcmp(effort, "3")==0)
		level=3;

	if(level==-1) {/* Try every combination in the user-defined list */
		next_opstring = effort-1;
		do {
			++next_opstring;
			if(qoip_encode(data, desc, working, working_len, next_opstring))
				return 1;
			if(tmp && currbest_len>*working_len) {/*scratch space used, copy best to out*/
				memcpy(out, tmp, *working_len);
				*out_len = *working_len;
			}
			qoipcrunch_update_stats(&currbest_len, currbest_str, working_len, next_opstring);
		} while( (next_opstring=strchr(next_opstring, ',')) );
		++cnt;
		if(count)
			*count=cnt;
		if(!tmp && *working_len!=currbest_len)/*scratch space not used, redo best combo*/
			qoip_encode(data, desc, out, working_len, currbest_str);
		return 0;
	}

	/* Do common opstrings */
	for(j=0;j<common_cnt;++j) {
		if(qoip_encode(data, desc, working, working_len, common[j]))
			return 1;
		if(tmp && currbest_len>*working_len) {
			memcpy(out, tmp, *working_len);
			*out_len = *working_len;
		}
		qoipcrunch_update_stats(&currbest_len, currbest_str, working_len, common[j]);
		++cnt;
	}
	if(level==0) {
		if(count)
			*count=cnt;
		if(!tmp && *working_len!=currbest_len)
			qoip_encode(data, desc, working, working_len, currbest_str);
		return 0;
	}

	do {/* Level 1-3 */
		opcnt=4;/* Reserve space for OP_RGB/OP_RGBA/RUN2/INDEX8 */
		for(j=0;j<set_cnt;++j)
			opcnt+=set[j].codespace[i[j]];
		if(opcnt>=192 && opcnt<256) {
			opstring_loc=0;
			opstring_loc+=sprintf(opstring+opstring_loc, "%02x", OP_INDEX8);
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
				if((standalone_use+opcnt)<256) {
					if(qoip_encode(data, desc, working, working_len, opstring2))
						return 1;
					if(tmp && currbest_len>*working_len) {
						memcpy(out, tmp, *working_len);
						*out_len = *working_len;
					}
					++cnt;
				}
			}
			qoipcrunch_update_stats(&currbest_len, currbest_str, working_len, opstring2);
		}
	} while(!qoipcrunch_iterate(level-1, set, set_cnt, i));

	if(count)
		*count=cnt;

	if(!tmp && *working_len!=currbest_len)
		qoip_encode(data, desc, out, working_len, currbest_str);
	return 0;
}

#endif /* QOIPCRUNCH_C */
