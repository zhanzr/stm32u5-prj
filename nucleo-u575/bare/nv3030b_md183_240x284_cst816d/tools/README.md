# tools

`png_to_rgb565.py` regenerates the bring-up asset from a PNG:

```bash
python png_to_rgb565.py <input.png> ../src/asset_test1.c asset_test1
```

Pure standard library (zlib only) - no Pillow/numpy needed.

`src/asset_test1.c` / `.h` are generated output; do not edit them by hand.
The checked-in asset is 64x64 RGB565 (4096 `uint16_t`), row-major, with
alpha composited over black so transparent/rounded pixels come out black.

If you do not have the original `test1.png`, the checked-in asset is
byte-identical to the one used by the ch32v307 reference and the tricore
ports, so it can be copied from there instead of regenerated.
