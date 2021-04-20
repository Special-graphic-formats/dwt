/*
dwt - playing with dwt and lossy image compression
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "dwt.h"

void blah(float *output, float *input, int N, int Q)
{
	for (int i = 0; i < N; i++)
		haar(output + 3 * N * i, input + 3 * N * i, N, 3);
	for (int i = 0; i < N; i++)
		haar(input + 3 * i, output + 3 * i, N, 3 * N);
	for (int i = 0; i < N * N; i++)
		output[i * 3] = nearbyintf(Q * input[i * 3]);
}

void doit(float *output, struct image *input, int *quant)
{
	int N = input->width;
	ycbcr_image(input);
	blah(output + 0, input->buffer + 0, N, quant[0]);
	blah(output + 1, input->buffer + 1, N, quant[1]);
	blah(output + 2, input->buffer + 2, N, quant[2]);
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
	float *output = malloc(sizeof(float) * 3 * input->total);
	int head[4] = { input->width, 128, 32, 32 };
	doit(output, input, head+1);
	FILE *file = fopen(argv[2], "w");
	if (!file) {
		fprintf(stderr, "could not open \"%s\" file to write.\n", argv[2]);
		return 1;
	}
	if (fwrite(head, sizeof(head), 1, file) != 1) {
		fprintf(stderr, "could not write to file \"%s\".\n", argv[2]);
		fclose(file);
		return 1;
	}
	for (int i = 0; i < 3 * input->total; i++) {
		int tmp = output[i];
		if (fwrite(&tmp, sizeof(tmp), 1, file) != 1) {
			fprintf(stderr, "could not write to file \"%s\".\n", argv[2]);
			fclose(file);
			return 1;
		}
	}
	fclose(file);
	return 0;
}

