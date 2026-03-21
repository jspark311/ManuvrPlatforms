/*
* ImagePNG_IO.cpp
*/

#include "../C3PLinux.h"
#include "Image/Image.h"

#if defined(CONFIG_C3P_WITH_LIBPNG)

/* libpng */
#include <png.h>

/* libc */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


ImageWriter::ImageWriter(Image* img, const char* filename) :
  _img(img),
  _filename(filename) {}


ImageWriter::~ImageWriter() {}


bool ImageWriter::writePNG() {
  if ((nullptr == _img) || (nullptr == _filename)) {
    return false;
  }
  if (!_img->allocated() || (0 == _img->width()) || (0 == _img->height())) {
    return false;
  }

  return _write_png_rgb888();
}


/*
* Convert a single row from Image into RGB888 using getPixel().
* Alpha is ignored by construction (we only take R,G,B from returned color).
*
* Assumption: getPixel() returns 0x00RRGGBB.
* If your getPixel() returns a different packing, change the shifts below.
*/
void ImageWriter::_convert_row_to_rgb888(uint8_t* dst_row, uint32_t y) {
  const uint32_t W = (uint32_t) _img->width();

  for (uint32_t x = 0; x < W; x++) {
    const uint32_t px = _img->getPixel((PixUInt) x, (PixUInt) y);

    const uint8_t r = (uint8_t) ((px >> 16) & 0xFF);
    const uint8_t g = (uint8_t) ((px >> 8)  & 0xFF);
    const uint8_t b = (uint8_t) (px & 0xFF);

    *(dst_row + (x * 3) + 0) = r;
    *(dst_row + (x * 3) + 1) = g;
    *(dst_row + (x * 3) + 2) = b;
  }
}


/*
* Always write PNG as 8-bit RGB, no alpha.
*
* Uses a single row buffer and png_write_row().
*/
bool ImageWriter::_write_png_rgb888() {
  bool ret = false;

  FILE* fp = nullptr;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;

  const uint32_t W = (uint32_t) _img->width();
  const uint32_t H = (uint32_t) _img->height();
  const uint32_t ROW_BYTES = W * 3;

  uint8_t row_buf[ROW_BYTES];
  memset(row_buf, 0, ROW_BYTES);

  fp = fopen(_filename, "wb");
  if (nullptr == fp) {
    return false;
  }

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (nullptr == png_ptr) {
    fclose(fp);
    return false;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (nullptr == info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    fclose(fp);
    return false;
  }

  if (0 != setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(
    png_ptr,
    info_ptr,
    (png_uint_32) W,
    (png_uint_32) H,
    8,                      /* bit depth */
    PNG_COLOR_TYPE_RGB,     /* color type */
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png_ptr, info_ptr);

  for (uint32_t y = 0; y < H; y++) {
    _convert_row_to_rgb888(row_buf, y);
    png_write_row(png_ptr, (png_bytep) row_buf);
  }
  png_write_end(png_ptr, info_ptr);

  ret = true;

  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
  return ret;
}




ImageReader::ImageReader(Image* img, const char* filename) :
  _img(img),
  _filename(filename) {}


ImageReader::~ImageReader() {}


/*
* Reads PNG and forces output to RGB888.
*
* Output contract:
*   - _img size set to PNG width/height
*   - _img buffer format set to ImgBufferFormat::R8_G8_B8
*   - alpha is discarded (stripped)
*/
bool ImageReader::readPNG() {
  bool ret = false;

  FILE* fp = nullptr;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;

  if ((nullptr == _img) || (nullptr == _filename)) {
    return false;
  }

  fp = fopen(_filename, "rb");
  if (nullptr == fp) {
    return false;
  }

  /* Quick signature check (optional but helpful). */
  {
    uint8_t sig[8];
    if (8 != fread(sig, 1, 8, fp)) {
      fclose(fp);
      return false;
    }
    if (0 != png_sig_cmp(sig, 0, 8)) {
      fclose(fp);
      return false;
    }
  }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (nullptr == png_ptr) {
    fclose(fp);
    return false;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (nullptr == info_ptr) {
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    fclose(fp);
    return false;
  }

  if (0 != setjmp(png_jmpbuf(png_ptr))) {
    /* libpng longjmp lands here on error */
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 width  = 0;
  png_uint_32 height = 0;
  int bit_depth = 0;
  int color_type = 0;
  int interlace_type = 0;
  int compression_type = 0;
  int filter_method = 0;

  png_get_IHDR(
    png_ptr, info_ptr,
    &width, &height,
    &bit_depth, &color_type,
    &interlace_type, &compression_type, &filter_method
  );

  /*
  * Transform pipeline to force:
  *   - 8-bit channels
  *   - RGB (no palette, no gray)
  *   - no alpha
  */
  if (16 == bit_depth) {
    png_set_strip_16(png_ptr);
  }

  if (PNG_COLOR_TYPE_PALETTE == color_type) {
    png_set_palette_to_rgb(png_ptr);
  }

  /* Expand grayscale 1/2/4 -> 8, and expand tRNS -> alpha (we strip alpha later). */
  if ((PNG_COLOR_TYPE_GRAY == color_type) || (PNG_COLOR_TYPE_GRAY_ALPHA == color_type)) {
    if (8 > bit_depth) {
      png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
  }
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
  }

  /* If gray, convert to RGB. */
  if ((PNG_COLOR_TYPE_GRAY == color_type) || (PNG_COLOR_TYPE_GRAY_ALPHA == color_type)) {
    png_set_gray_to_rgb(png_ptr);
  }

  /* If it has alpha, strip it. */
  if ((PNG_COLOR_TYPE_RGB_ALPHA == color_type) || (PNG_COLOR_TYPE_GRAY_ALPHA == color_type)) {
    png_set_strip_alpha(png_ptr);
  }
  else {
    /* It might gain alpha from tRNS expansion; strip in that case too. */
    png_set_strip_alpha(png_ptr);
  }

  /* Apply transforms. */
  png_read_update_info(png_ptr, info_ptr);

  /* After update, we expect RGB888. */
  const png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
  if (rowbytes != (png_size_t)(width * 3)) {
    /* Something unexpected; refuse rather than guess. */
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);
    return false;
  }

  /* Allocate output buffer (tightly packed RGB). */
  const uint32_t OUT_BYTES = (uint32_t)(width * height * 3);
  uint8_t out_buf[OUT_BYTES];

  /* Read row-by-row straight into out_buf. */
  for (png_uint_32 y = 0; y < height; y++) {
    png_bytep row_ptr = (png_bytep)(out_buf + (y * (width * 3)));
    png_read_row(png_ptr, row_ptr, nullptr);
  }

  png_read_end(png_ptr, nullptr);

  /* Now commit into Image as R8_G8_B8. */
  if (!_img->setSize((PixUInt) width, (PixUInt) height)) {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);
    return false;
  }

  /*
  * setBufferByCopy() claims its own heap buffer and copies the external data.
  * We free our staging buffer immediately after.
  */
  if (_img->setBufferByCopy(out_buf, ImgBufferFormat::R8_G8_B8)) {
    ret = true;
  }

  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
  fclose(fp);

  return ret;
}

#endif  // CONFIG_C3P_WITH_LIBPNG
