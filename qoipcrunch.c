/* SPDX-License-Identifier: MIT */
/* qoipcrunch - Reduce QOIP filesize by searching for better opcode combinations

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

int optmode_license(opt_t *opt) {
	printf("The MIT License(MIT)\n");
	printf("\n");
	printf("Copyright 2021 Dominic Szablewski (QOI format)\n");
	printf("Copyright 2021 Matthew Ling (QOIP format)\n");
	printf("\n");
	printf("Permission is hereby granted, free of charge, to any person obtaining a copy of\n");
	printf("this software and associated documentation files(the \"Software\"), to deal in\n");
	printf("the Software without restriction, including without limitation the rights to\n");
	printf("use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies\n");
	printf("of the Software, and to permit persons to whom the Software is furnished to do\n");
	printf("so, subject to the following conditions :\n");
	printf("The above copyright notice and this permission notice shall be included in all\n");
	printf("copies or substantial portions of the Software.\n");
	printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
	printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
	printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE\n");
	printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
	printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
	printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
	printf("SOFTWARE.\n");
	return 0;
}

/* -list option */
int optmode_list(opt_t *opt) {
	printf("Key:\na: Alpha verbatim\nvr, vg, vb, va: Channel difference from previous pixel\n");
	printf("avg_r, avg_g, avg_b: Channel difference from average of previous pixel and pixel above\n");
	printf("avg_gr, avg_gb: r/b channel difference from average relative to g\n");
	qoip_print_ops(stdout);
	return 0;
}

int main(int argc, char *argv[]) {
	opt_t opt;
	qoip_desc desc;
	unsigned char *raw, *tmp, *scratch;
	size_t tmp_len, max_size, raw_size;
	char effort_level[32];

	/* Process args */
	opt_init(&opt);
	opt_process(&opt, argc, argv);
	if(opt_dispatch(&opt))
		return 1;
	sprintf(effort_level, "%d", opt.effort);

	if(opt.in==NULL) {
		optmode_help(&opt);
		return 1;
	}

	/* Decode qoip to raw pixels */
	if(qoip_read_header((u8*)opt.in, NULL, &desc)) {
		printf("Failed to read header\n");
		return 1;
	}

	raw_size = qoip_maxsize_raw(&desc, desc.channels);
	raw = malloc(raw_size);
	assert(raw);

	max_size = qoip_maxsize(&desc) < qoip_maxentropysize(qoip_maxsize(&desc), opt.entropy) ? qoip_maxentropysize(qoip_maxsize(&desc), opt.entropy) : qoip_maxsize(&desc);
	tmp = malloc(max_size);
	assert(tmp);
	scratch = malloc(max_size*opt.threads);
	assert(scratch);

	qoip_decode(opt.in, opt.in_len, &desc, desc.channels, raw, scratch);

	if(qoipcrunch_encode(raw, &desc, tmp, &tmp_len, opt.custom?opt.custom:effort_level, scratch, opt.threads, opt.entropy))
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

	printf("Output size is %.2f%% of input%s\n", (tmp_len*100.0)/opt.in_len, (tmp_len<opt.in_len)?"":" (so not used)");

	free(raw);
	free(tmp);
	free(scratch);
	return 0;
}

