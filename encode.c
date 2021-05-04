/*
Encoder for lossy image compression based on the discrete wavelet transformation

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
		dwt2d(cdf97, output, input, lmin, length, 1, 1);
	else
		haar2d(output, input, lmin, length, 1, 1);
}

void quantization(float *values, int length, int len, int xoff, int yoff, int quant, int rounding)
{
	for (int y = 0; y < len; ++y) {
		for (int x = 0; x < len; ++x) {
			int idx = length * (yoff + y) + xoff + x;
			float v = values[idx];
			v *= quant;
			if (rounding)
				v = truncf(v);
			else
				v = nearbyintf(v);
			values[idx] = v;
		}
	}
}

void copy(float *output, float *input, int width, int height, int length, int col, int row, int stride)
{
	if (width == length && height == length) {
		for (int i = 0; i < length * length; ++i)
			output[i] = input[i*stride];
		return;
	}
	int cnt = 0;
	float sum = 0.f;
	for (int j = 0, y = length*row; j < length && y < height; ++j, ++y)
		for (int i = 0, x = length*col; i < length && x < width; ++i, ++x, ++cnt)
			sum += input[(width*y+x)*stride];
	float avg = sum / cnt;
	for (int j = 0, y = length*row; j < length; ++j, ++y)
		for (int i = 0, x = length*col; i < length; ++i, ++x)
			if (y < height && x < width)
				output[length*j+i] = input[(width*y+x)*stride];
			else
				output[length*j+i] = avg;
}

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 6 && argc != 7 && argc != 8 && argc != 9 && argc != 10) {
		fprintf(stderr, "usage: %s input.ppm output.dwt [Q0 Q1 Q2] [MODE] [WAVELET] [ROUNDING] [CAPACITY]\n", argv[0]);
		return 1;
	}
	struct image *image = read_ppm(argv[1]);
	if (!image)
		return 1;
	int width = image->width;
	int height = image->height;
	int lmin = 8;
	int length = lmin;
	while (length < width && length < height)
		length *= 2;
	if (length != width || length != height)
		length /= 2;
	int pixels = length * length;
	int rows = (height + length - 1) / length;
	int cols = (width + length - 1) / length;
	fprintf(stderr, "%d cols and %d rows of len %d\n", cols, rows, length);
	int quant[3] = { 128, 32, 32 };
	if (argc >= 6)
		for (int i = 0; i < 3; ++i)
			quant[i] = atoi(argv[3+i]);
	int mode = 1;
	if (argc >= 7)
		mode = atoi(argv[6]);
	int wavelet = 1;
	if (argc >= 8)
		wavelet = atoi(argv[7]);
	int rounding = 1;
	if (argc >= 9)
		rounding = atoi(argv[8]);
	int capacity = 1 << 23;
	if (argc >= 10)
		capacity = atoi(argv[9]);
	if (mode) {
		ycbcr_image(image);
		for (int i = 0; i < width * height; ++i)
			image->buffer[3*i] -= 0.5f;
	} else {
		for (int i = 0; i < 3 * width * height; ++i)
			image->buffer[i] -= 0.5f;
	}
	float *input = malloc(sizeof(float) * pixels);
	float *output = malloc(sizeof(float) * 3 * pixels * rows * cols);
	for (int j = 0; j < 3; ++j) {
		if (!quant[j])
			continue;
		for (int row = 0; row < rows; ++row) {
			for (int col = 0; col < cols; ++col) {
				float *values = output + pixels * ((cols * row + col) * 3 + j);
				copy(input, image->buffer+j, width, height, length, col, row, 3);
				transformation(values, input, length, lmin, wavelet);
			}
		}
	}
	delete_image(image);
	free(input);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_bit(bits, wavelet);
	put_bit(bits, rounding);
	put_vli(bits, width);
	put_vli(bits, height);
	put_vli(bits, lmin);
	for (int i = 0; i < 3; ++i)
		put_vli(bits, quant[i]);
	int qadj_max = 0;
	while ((!quant[0] || quant[0] >> qadj_max) &&
		(!quant[1] || quant[1] >> qadj_max) &&
		(!quant[2] || quant[2] >> qadj_max))
			++qadj_max;
	if (qadj_max > 0)
		--qadj_max;
	int qadj = 0;
	for (int len = lmin/2; len <= length/2; len *= 2) {
		bits_flush(bits);
		put_bit(bits, 1);
		put_vli(bits, qadj);
		for (int j = 0; j < 3; ++j) {
			if (!quant[j])
				continue;
			for (int row = 0; row < rows; ++row) {
				for (int col = 0; col < cols; ++col) {
					float *values = output + pixels * ((cols * row + col) * 3 + j);
					for (int yoff = 0; yoff < len*2; yoff += len) {
						for (int xoff = (!yoff && len >= lmin) * len; xoff < len*2; xoff += len) {
							quantization(values, length, len, xoff, yoff, quant[j] >> qadj, (xoff || yoff) && rounding);
							int last = 0;
							for (int i = 0; i < len*len; ++i) {
								struct position pos = hilbert(len, i);
								int idx = length * (yoff + pos.y) + xoff + pos.x;
								if (values[idx]) {
									if (i - last) {
										put_vli(bits, 0);
										put_vli(bits, i - last - 1);
									}
									last = i + 1;
									put_vli(bits, fabsf(values[idx]));
									put_bit(bits, values[idx] < 0.f);
								}
							}
							if (last < len*len) {
								put_vli(bits, 0);
								put_vli(bits, len*len - last - 1);
							}
						}
					}
				}
			}
		}
		int cnt = bits_count(bits);
		if (cnt >= capacity) {
			bits_discard(bits);
			fprintf(stderr, "%d bits over capacity, discarding %.1f%% of pixels\n", cnt-capacity+1, (100.f*(length*length-len*len))/(length*length));
			break;
		}
		qadj = (cnt * (length / len) - capacity) / capacity;
		qadj = qadj < 0 ? 0 : qadj > qadj_max ? qadj_max : qadj;
		if (qadj && len < length/4)
			fprintf(stderr, "adjusting quantization by %d in len %d\n", qadj, 2*len);
	}
	put_bit(bits, 0);
	fprintf(stderr, "%d bits encoded\n", bits_count(bits));
	close_writer(bits);
	return 0;
}

