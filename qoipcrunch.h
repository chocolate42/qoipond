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

int qoipcrunch_encode(const void *data, const qoip_desc *desc, void *out, size_t *out_len, char *effort, size_t *count, void *tmp) {
	char currbest_str[256], opstring[256], *next_opstring;
	int j;
	size_t currbest_len, w_len;
	size_t cnt = 0;
	int level = -1;
	int isrgb = desc->channels==3 ? 1 : 0;
	void *working = tmp?tmp:out;
	size_t *working_len = tmp?&w_len:out_len;

	char **list = isrgb?qoipcrunch_rgb:qoipcrunch_rgba;
	int list_cnt;

	int rgb_levels[]  = {1, 2, 6, 10, 15,  qoipcrunch_rgb_cnt};
	int rgba_levels[] = {1, 2, 6, 10, 15, qoipcrunch_rgba_cnt};

	currbest_len = qoip_maxsize(desc);

	if(!effort)
		effort="";

	if(     strcmp(effort, "0")==0)
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

	list_cnt = isrgb ? rgb_levels[level] : rgba_levels[level];
	for(j=0;j<list_cnt;++j) {
		if(qoip_encode(data, desc, working, working_len, list[j]))
			return 1;
		if(tmp && currbest_len>*working_len) {
			memcpy(out, tmp, *working_len);
			*out_len = *working_len;
		}
		++cnt;
		qoipcrunch_update_stats(&currbest_len, currbest_str, working_len, opstring);
	}

	if(count)
		*count=cnt;
	if(!tmp && *working_len!=currbest_len)
		qoip_encode(data, desc, out, working_len, currbest_str);
	return 0;
}

#endif /* QOIPCRUNCH_C */
