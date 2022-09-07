`ORG.SGF:`
![GitHub release (latest by date)](https://img.shields.io/github/v/release/Special-graphic-formats/dwt)
![GitHub Release Date](https://img.shields.io/github/release-date/Special-graphic-formats/dwt)
![GitHub repo size](https://img.shields.io/github/repo-size/Special-graphic-formats/dwt)
![GitHub all releases](https://img.shields.io/github/downloads/Special-graphic-formats/dwt/total)
![GitHub](https://img.shields.io/github/license/Special-graphic-formats/dwt)  

# DWT

### Playing with lossy and lossless image compression based on the discrete wavelet transformation

Quick start:

Encode [smpte.ppm](smpte.ppm) [PNM](https://en.wikipedia.org/wiki/Netpbm) picture file to ```encoded.dwt```:

```
./dwtenc smpte.ppm encoded.dwt
```

Decode ```encoded.dwt``` file to ```decoded.ppm``` picture file:

```
./dwtdec encoded.dwt decoded.ppm
```

Watch ```decoded.ppm``` picture file in [feh](https://feh.finalrewind.org/):

```
feh decoded.ppm
```

### Limited storage capacity

Use up to ```65536``` bits of space instead of the default ```0``` (no limit) and discard quality bits, if necessary, to stay below ```65536``` bits:

```
./dwtenc smpte.ppm encoded.dwt 65536
```

### Use different wavelet

Use the reversible integer [Haar wavelet](https://en.wikipedia.org/wiki/Haar_wavelet) instead of the default ```1``` [CDF](https://en.wikipedia.org/wiki/Cohen%E2%80%93Daubechies%E2%80%93Feauveau_wavelet) 9/7 wavelet for lossless compression:

```
./dwtenc smpte.ppm encoded.dwt 0 2
```

### Reading

* Run-length encodings  
by Solomon W. Golomb - 1966
* Image coding using wavelet transform  
by M. Antonini, M. Barlaud, P. Mathieu and I. Daubechies - 1992
* Factoring wavelet transforms into lifting steps  
by Ingrid Daubechies and Wim Sweldens - 1996
