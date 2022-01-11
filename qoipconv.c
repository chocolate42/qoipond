/* Command line tool to convert between png <> qoip format

Requires "stb_image.h" and "stb_image_write.h"
Compile with:
	gcc qoipconv.c -std=c99 -O3 -o qoipconv

Dominic Szablewski - https://phoboslab.org

-- LICENSE: The MIT License(MIT)

Copyright(c) 2021 Dominic Szablewski

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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOIPCRUNCH_C
#include "qoipcrunch.h"
#define QOIP_C
#include "qoip.h"
#define OPT_C
#include "qoipconv-opt.h"

#include <stdio.h>

/* Encode raw RGB or RGBA pixels into a QOIP image and write it to the file
system. The qoip_desc struct must be filled with the image width, height,
number of channels (3 = RGB, 4 = RGBA) and the colorspace.

The function returns 0 on failure (invalid parameters, or fopen or malloc
failed) or the number of bytes written on success. */
size_t qoip_write(const char *filename, const void *data, const qoip_desc *desc);

/* Read and decode a QOIP image from the file system. If channels is 0, the
number of channels from the file header is used. If channels is 3 or 4 the
output format will be forced into this number of channels.

The function either returns NULL on failure (invalid data, or malloc or fopen
failed) or a pointer to the decoded pixels. On success, the qoip_desc struct
will be filled with the description from the file header.

The returned pixel data should be free()d after use. */
void *qoip_read(const char *filename, qoip_desc *desc, int channels);

#ifndef QOIP_MALLOC
	#define QOIP_MALLOC(sz) malloc(sz)
	#define QOIP_FREE(p)    free(p)
#endif

size_t qoipcrunch_write(const char *filename, const void *data, const qoip_desc *desc, char *effort, int threads, int entropy) {
	FILE *f;
	size_t max_size, size;
	void *encoded, *scratch;

	max_size = qoip_maxsize(desc);
	max_size = max_size < qoipcrunch_maxentropysize(max_size, entropy) ? qoipcrunch_maxentropysize(max_size, entropy) : max_size;
	encoded = QOIP_MALLOC(max_size);
	scratch = QOIP_MALLOC(max_size*threads);
	if (
		!encoded || !scratch ||
		qoipcrunch_encode(data, desc, encoded, &size, effort, NULL, scratch, threads, entropy) ||
		!(f = fopen(filename, "wb"))
	) {
		QOIP_FREE(encoded);
		QOIP_FREE(scratch);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fclose(f);

	QOIP_FREE(encoded);
	QOIP_FREE(scratch);
	return size;
}

void *qoip_read(const char *filename, qoip_desc *desc, int channels) {
	FILE *f = fopen(filename, "rb");
	size_t max_size, size;
	void *pixels, *data;

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size == 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);
	data = QOIP_MALLOC(size);
	if (!data) {
		fclose(f);
		return NULL;
	}

	if( fread(data, 1, size, f)!=size ) {
		return 0;
	}
	fclose(f);

	qoip_read_header(data, NULL, desc);
	max_size = qoip_maxsize_raw(desc, channels);
	pixels = QOIP_MALLOC(max_size);
	if (
		!pixels ||
		qoip_decode(data, size, desc, channels, pixels)
	) {
		QOIP_FREE(pixels);
		return 0;
	}

	QOIP_FREE(data);
	return pixels;
}

#define STR_ENDS_WITH(S, E) (strcmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

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

int main(int argc, char **argv) {
	opt_t opt;
	char effort_level[2];

	/* Process args */
	opt_init(&opt);
	opt_process(&opt, argc, argv);
	if(opt_dispatch(&opt))
		return 1;
	else if(argc==1)
		optmode_help(&opt);
	sprintf(effort_level, "%d", opt.effort);

	if(!opt.in || !opt.out) {
		printf("Input and output files need to be defined\n");
		return 1;
	}

	void *pixels = NULL;
	int w, h, channels;
	if (STR_ENDS_WITH(opt.in, ".png")) {
		if(!stbi_info(opt.in, &w, &h, &channels)) {
			printf("Couldn't read header %s\n", opt.in);
			return 1;
		}

		// Force all odd encodings to be RGBA
		if(channels != 3) {
			channels = 4;
		}

		pixels = (void *)stbi_load(opt.in, &w, &h, NULL, channels);
	}
	else if (STR_ENDS_WITH(opt.in, ".qoip")) {
		qoip_desc desc;
		pixels = qoip_read(opt.in, &desc, 0);
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}

	if (pixels == NULL) {
		printf("Couldn't load/decode %s\n", opt.in);
		exit(1);
	}

	int encoded = 0;
	if (STR_ENDS_WITH(opt.out, ".png")) {
		encoded = stbi_write_png(opt.out, w, h, channels, pixels, 0);
	}
	else if (STR_ENDS_WITH(opt.out, ".qoip")) {
		encoded = qoipcrunch_write(opt.out, pixels, &(qoip_desc){
			.width = w,
			.height = h,
			.channels = channels,
			.colorspace = QOIP_SRGB
		}, (opt.custom?opt.custom:effort_level), opt.threads, opt.entropy);
	}

	if (!encoded) {
		printf("Couldn't write/encode %s\n", opt.out);
		return 1;
	}

	free(pixels);
	return 0;
}
