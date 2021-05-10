/*
Decoder for lossy image compression based on the discrete wavelet transformation

Copyright 2014 Ahmet Inan <xdsopl@gmail.com>
*/

#include "haar.h"
#include "cdf97.h"
#include "dwt.h"
#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void transformation(float *output, float *input, int length, int lmin, int wavelet)
{
	if (wavelet)
		idwt2d(icdf97, output, input, lmin, length, 1, 1);
	else
		ihaar2d(output, input, lmin, length, 1, 1);
}

void quantization(float *values, int length, int len, int xoff, int yoff, int quant, int rounding)
{
	for (int y = 0; y < len; ++y) {
		for (int x = 0; x < len; ++x) {
			int idx = length * (yoff + y) + xoff + x;
			float v = values[idx];
			if (rounding) {
				float bias = 0.375f;
				if (v < 0.f)
					v -= bias;
				else if (v > 0.f)
					v += bias;
			}
			v /= 1 << quant;
			values[idx] = v;
		}
	}
}

float flerpf(float a, float b, float x)
{
	return (1.f - x) * a + x * b;
}

void copy(float *output, float *input, int width, int height, int length, int col, int row, int cols, int rows, int stride)
{
	if (width == length && height == length) {
		for (int i = 0; i < length * length; ++i)
			output[i*stride] = input[i];
		return;
	}
	int xlen = (width + cols - 1) / cols;
	int ylen = (height + rows - 1) / rows;
	int xoff = (length - xlen) / 2;
	int yoff = (length - ylen) / 2;
	for (int j = !row*yoff, y = (height*row)/rows-!!row*yoff; j < length && y < height; ++j, ++y)
		for (int i = !col*xoff, x = (width*col)/cols-!!col*xoff; i < length && x < width; ++i, ++x)
			if ((!col || i >= 2*xoff) && (!row || j >= 2*yoff))
				output[(width*y+x)*stride] = input[length*j+i];
			else
				output[(width*y+x)*stride] = flerpf(output[(width*y+x)*stride], input[length*j+i], fclampf(i/(2.f*xoff), 0.f, 1.f) * fclampf(j/(2.f*yoff), 0.f, 1.f));
}

void decode(struct bits_reader *bits, float *values, int length, int len, int xoff, int yoff)
{
	for (int i = 0; i < len*len; ++i) {
		struct position pos = hilbert(len, i);
		int idx = length * (yoff + pos.y) + xoff + pos.x;
		int val = get_vli(bits);
		if (val) {
			if (get_bit(bits))
				val = -val;
			values[idx] = val;
		} else {
			i += get_vli(bits);
		}
	}
}

void decode_root(struct bits_reader *bits, float *values, int length, int len)
{
	for (int j = 0; j < len; ++j) {
		for (int i = 0; i < len; ++i) {
			int val = get_vli(bits);
			if (val && get_bit(bits))
				val = -val;
			values[length*j+i] = val;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.dwt output.ppm\n", argv[0]);
		return 1;
	}
	struct bits_reader *bits = bits_reader(argv[1]);
	if (!bits)
		return 1;
	int wavelet = get_bit(bits);
	int width = get_vli(bits);
	int height = get_vli(bits);
	int depth = get_vli(bits);
	int dmin = get_vli(bits);
	int cols = get_vli(bits);
	int rows = get_vli(bits);
	int quant[3];
	for (int chan = 0; chan < 3; ++chan)
		quant[chan] = get_vli(bits);
	int length = 1 << depth;
	int lmin = 1 << dmin;
	int pixels = length * length;
	float *input = malloc(sizeof(float) * 3 * pixels * rows * cols);
	for (int i = 0; i < 3 * pixels * rows * cols; ++i)
		input[i] = 0;
	for (int chan = 0; chan < 3; ++chan) {
		for (int row = 0; row < rows; ++row) {
			for (int col = 0; col < cols; ++col) {
				float *values = input + pixels * ((cols * row + col) * 3 + chan);
				decode_root(bits, values, length, lmin/2);
				quantization(values, length, lmin/2, 0, 0, quant[chan], 0);
			}
		}
	}
	for (int len = lmin/2; len <= length/2; len *= 2) {
		if (!get_bit(bits)) {
			int factor = length / len;
			width /= factor;
			height /= factor;
			float *tmp = malloc(sizeof(float) * 3 * len * len * rows * cols);
			for (int chan = 0; chan < 3; ++chan)
				for (int row = 0; row < rows; ++row)
					for (int col = 0; col < cols; ++col)
						for (int y = 0; y < len; ++y)
							for (int x = 0; x < len; ++x)
								tmp[len*(len*((cols*row+col)*3+chan)+y)+x] =
									input[length*(length*((cols*row+col)*3+chan)+y)+x] / factor;
			free(input);
			input = tmp;
			length = len;
			pixels = length * length;
			break;
		}
		int qadj = get_vli(bits);
		for (int chan = 0; chan < 3; ++chan) {
			if (!get_bit(bits))
				continue;
			for (int row = 0; row < rows; ++row) {
				for (int col = 0; col < cols; ++col) {
					float *values = input + pixels * ((cols * row + col) * 3 + chan);
					for (int yoff = 0; yoff < len*2; yoff += len) {
						for (int xoff = !yoff * len; xoff < len*2; xoff += len) {
							decode(bits, values, length, len, xoff, yoff);
							quantization(values, length, len, xoff, yoff, quant[chan] - qadj, 1);
						}
					}
				}
			}
		}
	}
	close_reader(bits);
	struct image *image = new_image(argv[2], width, height);
	float *output = malloc(sizeof(float) * pixels);
	for (int chan = 0; chan < 3; ++chan) {
		for (int row = 0; row < rows; ++row) {
			for (int col = 0; col < cols; ++col) {
				float *values = input + pixels * ((cols * row + col) * 3 + chan);
				if (length == lmin/2) {
					copy(image->buffer+chan, values, width, height, length, col, row, cols, rows, 3);
				} else {
					transformation(output, values, length, lmin, wavelet);
					copy(image->buffer+chan, output, width, height, length, col, row, cols, rows, 3);
				}
			}
		}
	}
	for (int i = 0; i < width * height; ++i)
		image->buffer[3*i] += 0.5f;
	rgb_image(image);
	if (!write_ppm(image))
		return 1;
	return 0;
}

