/*
Encoder for lossy image compression based on the discrete wavelet transformation

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
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

void quantization(int *output, float *input, int length, int lmin, int quant, int col, int row, int cols, int rows, int chan, int chans)
{
	for (int y = 0, *out = output+(lmin/2)*(lmin/2)*(cols*(rows*chan+row)+col); y < lmin/2; ++y) {
		for (int x = 0; x < lmin/2; ++x) {
			float v = input[length*y+x];
			v *= 1 << quant;
			*out++ = nearbyintf(v);
		}
	}
	output += (lmin/2) * (lmin/2) * cols * rows * chans;
	for (int len = lmin/2; len <= length/2; output += 3*len*len*cols*rows*chans, len *= 2) {
		for (int yoff = 0, *out = output+3*len*len*(cols*(rows*chan+row)+col); yoff < len*2; yoff += len) {
			for (int xoff = !yoff * len; xoff < len*2; xoff += len) {
				for (int i = 0; i < len*len; ++i) {
					struct position pos = hilbert(len, i);
					float v = input[length*(yoff+pos.y)+xoff+pos.x];
					v *= 1 << quant;
					*out++ = truncf(v);
				}
			}
		}
	}
}

void copy(float *output, float *input, int width, int height, int length, int col, int row, int cols, int rows, int stride)
{
	if (width == length && height == length) {
		for (int i = 0; i < length * length; ++i)
			output[i] = input[i*stride];
		return;
	}
	int xlen = (width + cols - 1) / cols;
	int ylen = (height + rows - 1) / rows;
	int xoff = (length - xlen) / 2;
	int yoff = (length - ylen) / 2;
	int w1 = width - 1, h1 = height - 1;
	for (int j = 0, y = (height*row)/rows+2*h1-yoff; j < length; ++j, ++y)
		for (int i = 0, x = (width*col)/cols+2*w1-xoff; i < length; ++i, ++x)
			output[length*j+i] = input[(width*(h1-abs(h1-y%(2*h1)))+w1-abs(w1-x%(2*w1)))*stride];
}

void encode(struct bits_writer *bits, int *val, int num, int plane, int planes)
{
	int last = 0, mask = 1 << plane;
	for (int i = 0; i < num; ++i) {
		if (val[i] & mask) {
			put_vli(bits, i - last);
			last = i + 1;
			if (plane == planes-1)
				val[i] ^= ~mask;
		}
	}
	put_vli(bits, num - last);
}

int ilog2(int x)
{
	int l = -1;
	for (; x > 0; x /= 2)
		++l;
	return l;
}

void encode_root(struct bits_writer *bits, int *val, int num)
{
	int max = 0;
	for (int i = 0; i < num; ++i)
		if (max < abs(val[i]))
			max = abs(val[i]);
	int cnt = 1 + ilog2(max);
	put_vli(bits, cnt);
	for (int i = 0; cnt && i < num; ++i) {
		write_bits(bits, abs(val[i]), cnt);
		if (val[i])
			put_bit(bits, val[i] < 0);
	}
}

int over_capacity(struct bits_writer *bits, int capacity)
{
	int cnt = bits_count(bits);
	if (cnt >= capacity) {
		bits_discard(bits);
		put_bit(bits, 0);
		fprintf(stderr, "%d bits over capacity, discarding.\n", cnt-capacity+1);
		return 1;
	}
	return 0;
}

int count_planes(int *val, int num)
{
	int neg = -1, pos = 0, nzo = 0;
	for (int i = 0; i < num; ++i) {
		nzo |= val[i];
		if (val[i] < 0)
			neg &= val[i];
		else
			pos |= val[i];
	}
	if (!nzo)
		return 0;
	int cnt = sizeof(int) * 8 - 1;
	while (cnt >= 0 && (neg&(1<<cnt)) && !(pos&(1<<cnt)))
		--cnt;
	return cnt + 2;
}

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 6 && argc != 7 && argc != 8) {
		fprintf(stderr, "usage: %s input.ppm output.dwt [Q0 Q1 Q2] [WAVELET] [CAPACITY]\n", argv[0]);
		return 1;
	}
	struct image *image = read_ppm(argv[1]);
	if (!image)
		return 1;
	int width = image->width;
	int height = image->height;
	int dmin = 2;
	int lmin = 1 << dmin;
	int depth = ilog2(width);
	int length = 1 << depth;
	int cols = 1;
	int rows = 1;
	if (width != height || width != length) {
		for (int best = -1, d = dmin, l = lmin; l <= width || l <= height; ++d, l *= 2) {
			int c = (width + l - 1) / l;
			int r = (height + l - 1) / l;
			while (c > 1 && (l-lmin/2)*c < width)
				++c;
			while (r > 1 && (l-lmin/2)*r < height)
				++r;
			if ((width < height && c > 3) || r > 3)
				continue;
			int o = l * l * c * r - width * height;
			if (best < 0 || o < best) {
				best = o;
				cols = c;
				rows = r;
				depth = d;
				length = 1 << d;
			}
		}
	}
	int pixels = length * length;
	fprintf(stderr, "%d cols and %d rows of len %d\n", cols, rows, length);
	int quant[3] = { 7, 5, 5 };
	if (argc >= 6)
		for (int chan = 0; chan < 3; ++chan)
			quant[chan] = atoi(argv[3+chan]);
	int wavelet = 1;
	if (argc >= 7)
		wavelet = atoi(argv[6]);
	int capacity = 1 << 23;
	if (argc >= 8)
		capacity = atoi(argv[7]);
	ycbcr_image(image);
	for (int i = 0; i < width * height; ++i)
		image->buffer[3*i] -= 0.5f;
	float *input = malloc(sizeof(float) * pixels);
	float *output = malloc(sizeof(float) * pixels);
	int *buffer = malloc(sizeof(int) * 3 * pixels * rows * cols);
	for (int chan = 0; chan < 3; ++chan) {
		for (int row = 0; row < rows; ++row) {
			for (int col = 0; col < cols; ++col) {
				copy(input, image->buffer+chan, width, height, length, col, row, cols, rows, 3);
				transformation(output, input, length, lmin, wavelet);
				quantization(buffer, output, length, lmin, quant[chan], col, row, cols, rows, chan, 3);
			}
		}
	}
	delete_image(image);
	free(input);
	free(output);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	put_bit(bits, wavelet);
	put_vli(bits, width);
	put_vli(bits, height);
	put_vli(bits, depth);
	put_vli(bits, dmin);
	put_vli(bits, cols);
	put_vli(bits, rows);
	for (int chan = 0; chan < 3; ++chan)
		put_vli(bits, quant[chan]);
	fprintf(stderr, "%d bits for meta data\n", bits_count(bits));
	bits_flush(bits);
	int pixels_root = (lmin/2) * (lmin/2) * cols * rows;
	for (int chan = 0; chan < 3; ++chan)
		encode_root(bits, buffer+pixels_root*chan, pixels_root);
	fprintf(stderr, "%d bits for root image\n", bits_count(bits));
	int planes_max = count_planes(buffer + pixels_root * 3, 3 * (pixels * rows * cols - pixels_root));
	put_vli(bits, planes_max);
	int layers_max = 24;
	int planes[3*layers_max];
	for (int i = 0; i < 3*layers_max; ++i)
		planes[i] = -1;
	for (int layers = 0; layers < layers_max; ++layers) {
		for (int len = lmin/2, num = len*len*cols*rows*3, *buf = buffer+num, layer = 0;
		len <= length/2 && layer <= layers; len *= 2, buf += 3*num, num = len*len*cols*rows*3, ++layer) {
			int init = 0;
			int chan = 0;
			if (planes[layer*3+chan] < 0) {
				init = 1;
				bits_flush(bits);
				put_bit(bits, 1);
				planes[layer*3+chan] = count_planes(buf, num);
				put_vli(bits, planes[layer*3+chan]);
			}
			for (int loops = 4, loop = 0; loop < loops; ++loop) {
				int plane = planes_max-1 - ((layers-layer)*loops+loop);
				if (plane >= 0 && plane < planes[layer*3+chan]) {
					if (!init) {
						bits_flush(bits);
						put_bit(bits, 1);
					}
					encode(bits, buf, num, plane, planes[layer*3+chan]);
				}
				if (over_capacity(bits, capacity))
					goto end;
			}
		}
		for (int len = lmin/2, num = len*len*cols*rows*3, *buf = buffer+num, layer = 0;
		len <= length/2 && layer <= layers; len *= 2, buf += 3*num, num = len*len*cols*rows*3, ++layer) {
			for (int loops = 4, loop = 0; loop < loops; ++loop) {
				for (int chan = 1; chan < 3; ++chan) {
					int init = 0;
					if (planes[layer*3+chan] < 0) {
						init = 1;
						bits_flush(bits);
						put_bit(bits, 1);
						planes[layer*3+chan] = count_planes(buf+chan*num, num);
						put_vli(bits, planes[layer*3+chan]);
					}
					int plane = planes_max-1 - ((layers-layer)*loops+loop);
					if (plane >= 0 && plane < planes[layer*3+chan]) {
						if (!init) {
							bits_flush(bits);
							put_bit(bits, 1);
						}
						encode(bits, buf+chan*num, num, plane, planes[layer*3+chan]);
					}
					if (over_capacity(bits, capacity))
						goto end;
				}
			}
		}
	}
end:
	free(buffer);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

