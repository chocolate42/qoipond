/* qoipcrunch - Reduce QOIP filesize by searching for better opcode combinations

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

#define QOIPCRUNCH_C
#include "qoipcrunch.h"
#define QOIP_C
#include "qoip.h"
#define OPT_C
#include "qoipcrunch-opt.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>



/* -list option */
int optmode_list(opt_t *opt) {
	qoip_print_ops(qoip_ops, stdout);
	return 0;
}

/* Detect RGB input and skip combinations with explicit alpha handling TODO */
int main(int argc, char *argv[]) {
	opt_t opt;
	qoip_desc desc;
	unsigned char *raw, *tmp, *scratch;
	size_t tmp_len, cnt;

	/* Process args */
	opt_init(&opt);
	opt_process(&opt, argc, argv);
	if(opt_dispatch(&opt))
		return 1;

	if(opt.in==NULL) {
		optmode_help(&opt);
		return 1;
	}

	/* Decode qoip to raw pixels */
	if(qoip_read_header((u8*)opt.in, NULL, &desc)) {
		return 1;
	}

	raw = malloc(qoip_maxsize_raw(&desc, desc.channels));
	assert(raw);
	qoip_decode(opt.in, opt.in_len, &desc, desc.channels, raw);
	tmp = malloc(qoip_maxsize(&desc));
	assert(tmp);
	scratch = malloc(qoip_maxsize(&desc));
	assert(scratch);

	if(qoipcrunch_encode(raw, &desc, tmp, &tmp_len, opt.effort, &cnt, scratch))
		return 1;

	if(opt.out) {
		if(tmp_len<opt.in_len) {
			fwrite(tmp, 1, tmp_len, opt.out);
		}
		else {
			fwrite(opt.in, 1, opt.in_len, opt.out);
		}
		fclose(opt.out);
	}

	printf("Tried %"PRIu64" combinations, reduced to %f%% of input\n", cnt, (tmp_len*100.0)/opt.in_len);
	free(raw);
	free(tmp);
	free(scratch);
	return 0;
}

