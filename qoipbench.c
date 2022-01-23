/* Simple benchmark suite for png, stbi and qoip

Requires libpng, "stb_image.h" and "stb_image_write.h"
Compile with:
	gcc qoipbench.c -std=gnu99 -lpng -O3 -o qoipbench

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

#include <stdio.h>
#include <dirent.h>
#include <png.h>

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
#include "qoipbench-opt.h"

// -----------------------------------------------------------------------------
// Cross platform high resolution timer
// From https://gist.github.com/ForeverZer0/0a4f80fc02b96e19380ebb7a3debbee5
#include <inttypes.h>
#include <stdint.h>
#if defined(__linux)
	#define HAVE_POSIX_TIMER
	#include <time.h>
	#ifdef CLOCK_MONOTONIC
		#define CLOCKID CLOCK_MONOTONIC
	#else
		#define CLOCKID CLOCK_REALTIME
	#endif
#elif defined(__APPLE__)
	#define HAVE_MACH_TIMER
	#include <mach/mach_time.h>
#elif defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

static uint64_t ns() {
	static uint64_t is_init = 0;
#if defined(__APPLE__)
		static mach_timebase_info_data_t info;
		if (0 == is_init) {
			mach_timebase_info(&info);
			is_init = 1;
		}
		uint64_t now;
		now = mach_absolute_time();
		now *= info.numer;
		now /= info.denom;
		return now;
#elif defined(__linux)
		static struct timespec linux_rate;
		if (0 == is_init) {
			clock_getres(CLOCKID, &linux_rate);
			is_init = 1;
		}
		uint64_t now;
		struct timespec spec;
		clock_gettime(CLOCKID, &spec);
		now = spec.tv_sec * 1.0e9 + spec.tv_nsec;
		return now;
#elif defined(_WIN32)
		static LARGE_INTEGER win_frequency;
		if (0 == is_init) {
			QueryPerformanceFrequency(&win_frequency);
			is_init = 1;
		}
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (uint64_t) ((1e9 * now.QuadPart)	/ win_frequency.QuadPart);
#endif
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define ERROR(...) printf("abort at line " TOSTRING(__LINE__) ": " __VA_ARGS__); printf("\n"); exit(1)

// -----------------------------------------------------------------------------
// libpng encode/decode wrappers
// Seriously, who thought this was a good abstraction for an API to read/write
// images?

typedef struct {
	int size;
	int capacity;
	unsigned char *data;
} libpng_write_t;

void libpng_encode_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
	libpng_write_t *write_data = (libpng_write_t*)png_get_io_ptr(png_ptr);
	if (write_data->size + length >= write_data->capacity) {
		ERROR("PNG write");
	}
	memcpy(write_data->data + write_data->size, data, length);
	write_data->size += length;
}

void *libpng_encode(void *pixels, int w, int h, int channels, int *out_len) {
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		ERROR("png_create_write_struct");
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		ERROR("png_create_info_struct");
	}

	if (setjmp(png_jmpbuf(png))) {
		ERROR("png_jmpbuf");
	}

	// Output is 8bit depth, RGBA format.
	png_set_IHDR(
		png,
		info,
		w, h,
		8,
		channels == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	png_bytep row_pointers[h];
	for(int y = 0; y < h; y++){
		row_pointers[y] = ((unsigned char *)pixels + y * w * channels);
	}

	libpng_write_t write_data = {
		.size = 0,
		.capacity = w * h * channels,
		.data = malloc(w * h * channels)
	};

	png_set_rows(png, info, row_pointers);
	png_set_write_fn(png, &write_data, libpng_encode_callback, NULL);
	png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

	png_destroy_write_struct(&png, &info);

	*out_len = write_data.size;
	return write_data.data;
}

typedef struct {
	int pos;
	int size;
	unsigned char *data;
} libpng_read_t;

void png_decode_callback(png_structp png, png_bytep data, png_size_t length) {
	libpng_read_t *read_data = (libpng_read_t*)png_get_io_ptr(png);
	if (read_data->pos + length > read_data->size) {
		ERROR("PNG read %lu bytes at pos %d (size: %d)", length, read_data->pos, read_data->size);
	}
	memcpy(data, read_data->data + read_data->pos, length);
	read_data->pos += length;
}

void png_warning_callback(png_structp png_ptr, png_const_charp warning_msg) {
	// Ignore warnings about sRGB profiles and such.
}

void *libpng_decode(void *data, int size, int *out_w, int *out_h) {
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, png_warning_callback);
	if (!png) {
		ERROR("png_create_read_struct");
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		ERROR("png_create_info_struct");
	}

	libpng_read_t read_data = {
		.pos = 0,
		.size = size,
		.data = data
	};

	png_set_read_fn(png, &read_data, png_decode_callback);
	png_set_sig_bytes(png, 0);
	png_read_info(png, info);

	png_uint_32 w, h;
	int bitDepth, colorType, interlaceType;
	png_get_IHDR(png, info, &w, &h, &bitDepth, &colorType, &interlaceType, NULL, NULL);

	// 16 bit -> 8 bit
	png_set_strip_16(png);

	// 1, 2, 4 bit -> 8 bit
	if (bitDepth < 8) {
		png_set_packing(png);
	}

	if (colorType & PNG_COLOR_MASK_PALETTE) {
		png_set_expand(png);
	}

	if (!(colorType & PNG_COLOR_MASK_COLOR)) {
		png_set_gray_to_rgb(png);
	}

	// set paletted or RGB images with transparency to full alpha so we get RGBA
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}

	// make sure every pixel has an alpha value
	if (!(colorType & PNG_COLOR_MASK_ALPHA)) {
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	png_read_update_info(png, info);

	unsigned char* out = malloc(w * h * 4);
	*out_w = w;
	*out_h = h;

	// png_uint_32 rowBytes = png_get_rowbytes(png, info);
	png_bytep row_pointers[h];
	for (png_uint_32 row = 0; row < h; row++ ) {
		row_pointers[row] = (png_bytep)(out + (row * w * 4));
	}

	png_read_image(png, row_pointers);
	png_read_end(png, info);
	png_destroy_read_struct( &png, &info, NULL);

	return out;
}

// -----------------------------------------------------------------------------
// stb_image encode callback

void stbi_write_callback(void *context, void *data, int size) {
	int *encoded_size = (int *)context;
	*encoded_size += size;
	// In theory we'd need to do another malloc(), memcpy() and free() here to
	// be fair to the other decode functions...
}

// -----------------------------------------------------------------------------
// function to load a whole file into memory

void *fload(const char *path, int *out_size) {
	FILE *fh = fopen(path, "rb");
	if (!fh) {
		ERROR("Can't open file");
	}

	fseek(fh, 0, SEEK_END);
	int size = ftell(fh);
	fseek(fh, 0, SEEK_SET);

	void *buffer = malloc(size);
	if (!buffer) {
		ERROR("Malloc for %d bytes failed", size);
	}

	if (!fread(buffer, size, 1, fh)) {
		ERROR("Can't read file %s", path);
	}
	fclose(fh);

	*out_size = size;
	return buffer;
}

// -----------------------------------------------------------------------------
// benchmark runner

typedef struct {
	uint64_t size;
	uint64_t encode_time;
	uint64_t decode_time;
} benchmark_lib_result_t;

typedef struct {
	int count;
	uint64_t raw_size;
	uint64_t px;
	int w;
	int h;
	benchmark_lib_result_t libpng;
	benchmark_lib_result_t stbi;
	benchmark_lib_result_t qoip;
} benchmark_result_t;

void benchmark_print_result(opt_t *opt, char *effort, benchmark_result_t res) {
	res.px /= res.count;
	res.raw_size /= res.count;
	res.libpng.encode_time /= res.count;
	res.libpng.decode_time /= res.count;
	res.libpng.size /= res.count;
	res.stbi.encode_time /= res.count;
	res.stbi.decode_time /= res.count;
	res.stbi.size /= res.count;
	res.qoip.encode_time /= res.count;
	res.qoip.decode_time /= res.count;
	res.qoip.size /= res.count;

	double px = res.px;
	printf("decode_ms  encode_ms  decode_mpps  encode_mpps  size_kb  rate\n");
	if (opt->png) {
		printf(
			" %8.3f   %8.3f     %8.2f     %8.2f %8"PRIu64"  %4.1f%%: libpng\n",
			(double)res.libpng.decode_time/1000000.0,
			(double)res.libpng.encode_time/1000000.0,
			(res.libpng.decode_time > 0 ? px / ((double)res.libpng.decode_time/1000.0) : 0),
			(res.libpng.encode_time > 0 ? px / ((double)res.libpng.encode_time/1000.0) : 0),
			res.libpng.size/1024,
			((double)res.libpng.size/(double)res.raw_size) * 100.0
		);
		printf(
			" %8.3f   %8.3f     %8.2f     %8.2f %8"PRIu64"  %4.1f%%: stbi\n",
			(double)res.stbi.decode_time/1000000.0,
			(double)res.stbi.encode_time/1000000.0,
			(res.stbi.decode_time > 0 ? px / ((double)res.stbi.decode_time/1000.0) : 0),
			(res.stbi.encode_time > 0 ? px / ((double)res.stbi.encode_time/1000.0) : 0),
			res.stbi.size/1024,
			((double)res.stbi.size/(double)res.raw_size) * 100.0
		);
	}
	printf(
			" %8.3f   %8.3f     %8.2f     %8.2f %8"PRIu64"  %4.1f%%: qoip(%s).threads(%d).entropy(%d).smart(%d)\n",
		(double)res.qoip.decode_time/1000000.0,
		(double)res.qoip.encode_time/1000000.0,
		(res.qoip.decode_time > 0 ? px / ((double)res.qoip.decode_time/1000.0) : 0),
		(res.qoip.encode_time > 0 ? px / ((double)res.qoip.encode_time/1000.0) : 0),
		res.qoip.size/1024,
		((double)res.qoip.size/(double)res.raw_size) * 100.0,
		effort, opt->threads, opt->entropy, opt->smart
	);
	printf("\n");
}

// Run __VA_ARGS__ a number of times and meassure the time taken. The first
// run is ignored.
#define BENCHMARK_FN(NOWARMUP, RUNS, AVG_TIME, ...) \
	do { \
		uint64_t time = 0; \
		for (int i = NOWARMUP; i <= RUNS; i++) { \
			uint64_t time_start = ns(); \
			__VA_ARGS__ \
			uint64_t time_end = ns(); \
			if (i > 0) { \
				time += time_end - time_start; \
			} \
		} \
		AVG_TIME = time / RUNS; \
	} while (0)

benchmark_result_t benchmark_image(opt_t *opt, char *effort, const char *path) {
	int encoded_png_size;
	size_t encoded_qoip_size;
	int w;
	int h;
	int channels;
	void *encoded_png, *encoded_qoip, *pixels, *scratch;

	// Load the encoded PNG, encoded QOIP and raw pixels into memory
	if(!stbi_info(path, &w, &h, &channels)) {
		ERROR("Error decoding header %s", path);
	}

	if (channels != 3)
		channels = 4;

	pixels = (void *)stbi_load(path, &w, &h, NULL, channels);
	encoded_png = fload(path, &encoded_png_size);
	qoip_desc desc_raw;
	desc_raw.width = w;
	desc_raw.height = h;
	desc_raw.channels = channels;
	desc_raw.colorspace = QOIP_SRGB;
	size_t qoip_max_size = qoip_maxsize(&desc_raw);
	qoip_max_size = qoip_max_size < qoip_maxentropysize(qoip_max_size, opt->entropy) ? qoip_maxentropysize(qoip_max_size, opt->entropy) : qoip_max_size;
	encoded_qoip = malloc(qoip_max_size);
	scratch = malloc(qoip_max_size*opt->threads);
	if (!encoded_qoip || !scratch) {
		ERROR("Error, malloc failed %s", path);
	}
	if(opt->smart) {
		if (qoipcrunch_encode_smart(pixels, &desc_raw, encoded_qoip, &encoded_qoip_size, effort, NULL, scratch, opt->threads, opt->entropy)) {
			ERROR("Error, qoipcrunch_encode_smart failed %s", path);
		}
	}
	else {
		if (qoipcrunch_encode(pixels, &desc_raw, encoded_qoip, &encoded_qoip_size, effort, NULL, scratch, opt->threads, opt->entropy)) {
			ERROR("Error, qoipcrunch_encode failed %s", path);
		}
	}
	if (!pixels || !encoded_qoip || !encoded_png) {
		ERROR("Error decoding %s", path);
	}

	// Read header to populate qoi_desc from encoded file and malloc dest
	qoip_desc desc_enc;
	if (qoip_read_header(encoded_qoip, NULL, &desc_enc)) {
		ERROR("Error, header read failed %s", path);
	}
	// Set channels to 4 to reuse pixels_qoip as benchmark decode sink
	size_t pixels_qoip_size = qoip_maxsize_raw(&desc_enc, 4);
	void *pixels_qoip = malloc(pixels_qoip_size);
	if (!pixels_qoip) {
		ERROR("Error, malloc failed %s", path);
	}

	// Verify QOIP Output
	if (opt->verify) {
		if(qoip_decode(encoded_qoip, encoded_qoip_size, &desc_enc, channels, pixels_qoip, scratch)) {
			ERROR("Error, verify qoip_decode failed %s", path);
		}
		if (memcmp(pixels, pixels_qoip, w * h * channels) != 0) {
			ERROR("QOIP roundtrip pixel missmatch for %s", path);
		}
	}

	benchmark_result_t res = {0};
	res.count = 1;
	res.raw_size = w * h * channels;
	res.px = w * h;
	res.w = w;
	res.h = h;

	// Decoding
	if (opt->decode) {
		if (opt->png) {
			BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.libpng.decode_time, {
				int dec_w, dec_h;
				void *dec_p = libpng_decode(encoded_png, encoded_png_size, &dec_w, &dec_h);
				free(dec_p);
			});

			BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.stbi.decode_time, {
				int dec_w, dec_h, dec_channels;
				void *dec_p = stbi_load_from_memory(encoded_png, encoded_png_size, &dec_w, &dec_h, &dec_channels, 4);
				free(dec_p);
			});
		}

		BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.qoip.decode_time, {
			qoip_desc desc;
			if(qoip_decode(encoded_qoip, encoded_qoip_size, &desc, 4, pixels_qoip, scratch)) {
				ERROR("Error, qoip_decode failed %s", path);
			}
		});
	}

	free(pixels_qoip);

	// Encoding
	if (opt->encode) {
		if (opt->png) {
			BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.libpng.encode_time, {
				int enc_size;
				void *enc_p = libpng_encode(pixels, w, h, channels, &enc_size);
				res.libpng.size = enc_size;
				free(enc_p);
			});

			BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.stbi.encode_time, {
				int enc_size = 0;
				stbi_write_png_to_func(stbi_write_callback, &enc_size, w, h, channels, pixels, 0);
				res.stbi.size = enc_size;
			});
		}

		BENCHMARK_FN(opt->warmup?0:1, opt->iterations, res.qoip.encode_time, {
			size_t enc_size;
			if(opt->smart) {
				if (qoipcrunch_encode_smart(pixels, &desc_raw, encoded_qoip, &enc_size, effort, NULL, scratch, opt->threads, opt->entropy)) {
					ERROR("Error, qoipcrunch_encode_smart failed %s", path);
				}
			}
			else {
				if (qoipcrunch_encode(pixels, &desc_raw, encoded_qoip, &enc_size, effort, NULL, scratch, opt->threads, opt->entropy)) {
					ERROR("Error, qoipcrunch_encode failed %s", path);
				}
			}
			res.qoip.size = enc_size;
		});
	}

	free(pixels);
	free(encoded_png);
	free(encoded_qoip);
	free(scratch);

	return res;
}

void benchmark_directory(opt_t *opt, char *effort, const char *path, benchmark_result_t *grand_total) {
	DIR *dir = opendir(path);
	if (!dir) {
		ERROR("Couldn't open directory %s", path);
	}

	struct dirent *file;

	if (opt->recurse) {
		for (int i = 0; (file = readdir(dir)) != NULL; i++) {
			if (
				file->d_type & DT_DIR &&
				strcmp(file->d_name, ".") != 0 &&
				strcmp(file->d_name, "..") != 0
			) {
				char subpath[1024];
				snprintf(subpath, 1024, "%s/%s", path, file->d_name);
				benchmark_directory(opt, effort, subpath, grand_total);
			}
		}
		rewinddir(dir);
	}

	benchmark_result_t dir_total = {0};

	int has_shown_heaad = 0;
	for (int i = 0; (file = readdir(dir)) != NULL; i++) {
		if (strcmp(file->d_name + strlen(file->d_name) - 4, ".png") != 0) {
			continue;
		}

		if (!has_shown_heaad) {
			has_shown_heaad = 1;
			printf("## Benchmarking %s/*.png -- %d runs\n\n", path, opt->iterations);
		}

		char *file_path = malloc(strlen(file->d_name) + strlen(path)+8);
		sprintf(file_path, "%s/%s", path, file->d_name);

		benchmark_result_t res = benchmark_image(opt, effort, file_path);

		if (!opt->onlytotals) {
			printf("## %s size: %dx%d\n", file_path, res.w, res.h);
			benchmark_print_result(opt, effort, res);
		}

		free(file_path);

		dir_total.count++;
		dir_total.raw_size += res.raw_size;
		dir_total.px += res.px;
		dir_total.libpng.encode_time += res.libpng.encode_time;
		dir_total.libpng.decode_time += res.libpng.decode_time;
		dir_total.libpng.size += res.libpng.size;
		dir_total.stbi.encode_time += res.stbi.encode_time;
		dir_total.stbi.decode_time += res.stbi.decode_time;
		dir_total.stbi.size += res.stbi.size;
		dir_total.qoip.encode_time += res.qoip.encode_time;
		dir_total.qoip.decode_time += res.qoip.decode_time;
		dir_total.qoip.size += res.qoip.size;

		grand_total->count++;
		grand_total->raw_size += res.raw_size;
		grand_total->px += res.px;
		grand_total->libpng.encode_time += res.libpng.encode_time;
		grand_total->libpng.decode_time += res.libpng.decode_time;
		grand_total->libpng.size += res.libpng.size;
		grand_total->stbi.encode_time += res.stbi.encode_time;
		grand_total->stbi.decode_time += res.stbi.decode_time;
		grand_total->stbi.size += res.stbi.size;
		grand_total->qoip.encode_time += res.qoip.encode_time;
		grand_total->qoip.decode_time += res.qoip.decode_time;
		grand_total->qoip.size += res.qoip.size;
	}
	closedir(dir);

	if (dir_total.count > 0) {
		printf("## Total for %s\n", path);
		benchmark_print_result(opt, effort, dir_total);
	}
}

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

	if(!opt.directory) {
		printf("Pass directory to benchmark\n");
		return 1;
	}

	benchmark_result_t grand_total = {0};
	benchmark_directory(&opt, opt.custom?opt.custom:effort_level, opt.directory, &grand_total);

	if (grand_total.count > 0) {
		printf("# Grand total for %s\n", opt.directory);
		benchmark_print_result(&opt, opt.custom?opt.custom:effort_level, grand_total);
	}
	else
		printf("No images found in %s\n", opt.directory);

	return 0;
}
