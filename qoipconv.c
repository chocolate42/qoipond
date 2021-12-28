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

#define QOIP_C
#include "qoip.h"

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

size_t qoip_write(const char *filename, const void *data, const qoip_desc *desc) {
	FILE *f;
	size_t max_size, size;
	void *encoded;

	max_size = qoip_maxsize(desc);
	encoded = QOIP_MALLOC(max_size);
	if (
		!encoded ||
		qoip_encode(data, desc, encoded, &size, NULL) ||
		!(f = fopen(filename, "wb"))
	) {
		QOIP_FREE(encoded);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fclose(f);

	QOIP_FREE(encoded);
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

int main(int argc, char **argv) {

	if (argc < 3) {
		printf("Usage: qoipconv <infile> <outfile>\n");
		printf("Examples:\n");
		printf("  qoipconv input.png output.qoip\n");
		printf("  qoipconv input.qoip output.png\n");
		return 1;;
	}

	void *pixels = NULL;
	int w, h, channels;
	if (STR_ENDS_WITH(argv[1], ".png")) {
		if(!stbi_info(argv[1], &w, &h, &channels)) {
			printf("Couldn't read header %s\n", argv[1]);
			return 1;
		}

		// Force all odd encodings to be RGBA
		if(channels != 3) {
			channels = 4;
		}

		pixels = (void *)stbi_load(argv[1], &w, &h, NULL, channels);
	}
	else if (STR_ENDS_WITH(argv[1], ".qoip")) {
		qoip_desc desc;
		pixels = qoip_read(argv[1], &desc, 0);
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}

	if (pixels == NULL) {
		printf("Couldn't load/decode %s\n", argv[1]);
		exit(1);
	}

	int encoded = 0;
	if (STR_ENDS_WITH(argv[2], ".png")) {
		encoded = stbi_write_png(argv[2], w, h, channels, pixels, 0);
	}
	else if (STR_ENDS_WITH(argv[2], ".qoip")) {
		encoded = qoip_write(argv[2], pixels, &(qoip_desc){
			.width = w,
			.height = h,
			.channels = channels,
			.colorspace = QOIP_SRGB
		});
	}

	if (!encoded) {
		printf("Couldn't write/encode %s\n", argv[2]);
		return 1;
	}

	free(pixels);
	return 0;
}
