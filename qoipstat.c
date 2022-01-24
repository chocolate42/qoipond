/* SPDX-License-Identifier: MIT */
/* qoipstat - Display information from the header of a QOIP file

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

#define QOIP_C
#include "qoip.h"
#define OPT_C
#include "qoipstat-opt.h"
#include <stdio.h>

int optmode_license(opt_t *opt) {
	printf("The MIT License(MIT)\n");
	printf("\n");
	printf("Copyright(c) 2021 Dominic Szablewski (original QOI format)\n");
	printf("Copyright(c) 2021 Matthew Ling (adaptations for QOIP format)\n");
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

	/* Process args */
	opt_init(&opt);
	opt_process(&opt, argc, argv);
	if(opt_dispatch(&opt))
		return 1;

	if(opt.in==NULL) {
		optmode_help(&opt);
		return 1;
	}

	qoip_stat(opt.in, stdout);

	return 0;
}

