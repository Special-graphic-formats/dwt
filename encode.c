/*
dwt - playing with dwt and lossy image compression
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "dwt.h"
#include "ppm.h"
#include "vli.h"
#include "bits.h"

void blah(float *output, float *input, int N, int Q)
{
	haar2(output, input, N, 3);
	for (int i = 0; i < N * N; i++)
		output[i * 3] = nearbyintf(Q * output[i * 3]);
}

void doit(float *output, struct image *input, int *quant)
{
	int N = input->width;
	blah(output + 0, input->buffer + 0, N, quant[0]);
	blah(output + 1, input->buffer + 1, N, quant[1]);
	blah(output + 2, input->buffer + 2, N, quant[2]);
}

int pow2(int N)
{
	return !(N & (N - 1));
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.ppm output.dwt\n", argv[0]);
		return 1;
	}
	struct image *input = read_ppm(argv[1]);
	if (!input || input->width != input->height || !pow2(input->width))
		return 1;
	int mode = 1;
	int length = input->width;
	int pixels = 3 * length * length;
	int quant[3] = { 128, 32, 32 };
	float *output = malloc(sizeof(float) * pixels);
	if (mode)
		ycbcr_image(input);
	doit(output, input, quant);
	struct bits *bits = bits_writer(argv[2]);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, length);
	for (int i = 0; i < 3; ++i)
		put_vli(bits, quant[i]);
	for (int i = 0; i < pixels; i++) {
		if (output[i]) {
			put_vli(bits, fabsf(output[i]));
			put_bit(bits, output[i] < 0.f);
		} else {
			put_vli(bits, 0);
			int k = i + 1;
			while (k < pixels && !output[k])
				++k;
			--k;
			put_vli(bits, k - i);
			i = k;
		}
	}
	close_writer(bits);
	return 0;
}

