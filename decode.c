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

void doit(float *output, float *input, int length, int lmin, int quant, int qmin, int wavelet)
{
	for (int j = 0; j < length; ++j)
		for (int i = 0; i < length; ++i)
			input[length*j+i] /= (i < lmin && j < lmin ? qmin : quant);
	if (wavelet)
		idwt2d(icdf97, output, input, lmin, length, 3, 1);
	else
		ihaar2d(output, input, lmin, length, 3, 1);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.dwt output.ppm\n", argv[0]);
		return 1;
	}
	struct bits *bits = bits_reader(argv[1]);
	if (!bits)
		return 1;
	int mode = get_bit(bits);
	int wavelet = get_bit(bits);
	int length = get_vli(bits);
	int lmin = get_vli(bits);
	int pixels = length * length;
	int quant[3];
	for (int i = 0; i < 3; ++i)
		quant[i] = get_vli(bits);
	int qmin[3];
	for (int i = 0; i < 3; ++i)
		qmin[i] = get_vli(bits);
	float *input = malloc(sizeof(float) * pixels);
	struct image *output = new_image(argv[2], length, length);
	for (int j = 0; j < 3; ++j) {
		if (!quant[j]) {
			for (int i = 0; i < pixels; ++i)
				output->buffer[3*i+j] = 0;
			continue;
		}
		for (int i = 0; i < pixels; ++i) {
			float val = get_vli(bits);
			if (val) {
				if (get_bit(bits))
					val = -val;
			} else {
				int cnt = get_vli(bits);
				for (int k = 0; k < cnt; ++k)
					input[hilbert(length, i++)] = 0;
			}
			input[hilbert(length, i)] = val;
		}
		doit(output->buffer+j, input, length, lmin, quant[j], qmin[j], wavelet);
	}
	close_reader(bits);
	if (mode)
		rgb_image(output);
	if (!write_ppm(output))
		return 1;
	return 0;
}

