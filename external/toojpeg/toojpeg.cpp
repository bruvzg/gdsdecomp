// //////////////////////////////////////////////////////////
// toojpeg.cpp
// written by Stephan Brumme, 2018-2019
// see https://create.stephan-brumme.com/toojpeg/
//

#include "toojpeg.h"

// - the "official" specifications: https://www.w3.org/Graphics/JPEG/itu-t81.pdf and https://www.w3.org/Graphics/JPEG/jfif3.pdf
// - Wikipedia has a short description of the JFIF/JPEG file format: https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
// - the popular STB Image library includes Jon's JPEG encoder as well: https://github.com/nothings/stb/blob/master/stb_image_write.h
// - the most readable JPEG book (from a developer's perspective) is Miano's "Compressed Image File Formats" (1999, ISBN 0-201-60443-4),
//   used copies are really cheap nowadays and include a CD with C++ sources as well (plus great format descriptions of GIF & PNG)
// - much more detailled is Mitchell/Pennebaker's "JPEG: Still Image Data Compression Standard" (1993, ISBN 0-442-01272-1)
//   which contains the official JPEG standard, too - fun fact: I bought a signed copy in a second-hand store without noticing

namespace // anonymous namespace to hide local functions / constants / etc.
{
// ////////////////////////////////////////
// data types
using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using  int16_t =          short;
using  int32_t =          int; // at least four bytes

// ////////////////////////////////////////
// constants

// quantization tables from JPEG Standard, Annex K
const uint8_t DefaultQuantLuminance[8*8] =
    { 16, 11, 10, 16, 24, 40, 51, 61, // there are a few experts proposing slightly more efficient values,
      12, 12, 14, 19, 26, 58, 60, 55, // e.g. https://www.imagemagick.org/discourse-server/viewtopic.php?t=20333
      14, 13, 16, 24, 40, 57, 69, 56, // btw: Google's Guetzli project optimizes the quantization tables per image
      14, 17, 22, 29, 51, 87, 80, 62,
      18, 22, 37, 56, 68,109,103, 77,
      24, 35, 55, 64, 81,104,113, 92,
      49, 64, 78, 87,103,121,120,101,
      72, 92, 95, 98,112,100,103, 99 };
const uint8_t DefaultQuantChrominance[8*8] =
    { 17, 18, 24, 47, 99, 99, 99, 99,
      18, 21, 26, 66, 99, 99, 99, 99,
      24, 26, 56, 99, 99, 99, 99, 99,
      47, 66, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99 };

// 8x8 blocks are processed in zig-zag order
// most encoders use a zig-zag "forward" table, I switched to its inverse for performance reasons
// note: ZigZagInv[ZigZag[i]] = i
const uint8_t ZigZagInv[8*8] =
    {  0, 1, 8,16, 9, 2, 3,10,   // ZigZag[] =  0, 1, 5, 6,14,15,27,28,
      17,24,32,25,18,11, 4, 5,   //             2, 4, 7,13,16,26,29,42,
      12,19,26,33,40,48,41,34,   //             3, 8,12,17,25,30,41,43,
      27,20,13, 6, 7,14,21,28,   //             9,11,18,24,31,40,44,53,
      35,42,49,56,57,50,43,36,   //            10,19,23,32,39,45,52,54,
      29,22,15,23,30,37,44,51,   //            20,22,33,38,46,51,55,60,
      58,59,52,45,38,31,39,46,   //            21,34,37,47,50,56,59,61,
      53,60,61,54,47,55,62,63 }; //            35,36,48,49,57,58,62,63

// static Huffman code tables from JPEG standard Annex K
// - CodesPerBitsize tables define how many Huffman codes will have a certain bitsize (plus 1 because there nothing with zero bits),
//   e.g. DcLuminanceCodesPerBitsize[2] = 5 because there are 5 Huffman codes being 2+1=3 bits long
// - Values tables are a list of values ordered by their Huffman code bitsize,
//   e.g. AcLuminanceValues => Huffman(0x01,0x02 and 0x03) will have 2 bits, Huffman(0x00) will have 3 bits, Huffman(0x04,0x11 and 0x05) will have 4 bits, ...

// Huffman definitions for first DC/AC tables (luminance / Y channel)
const uint8_t DcLuminanceCodesPerBitsize[16]   = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };   // sum = 12
const uint8_t DcLuminanceValues         [12]   = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes
const uint8_t AcLuminanceCodesPerBitsize[16]   = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 }; // sum = 162
const uint8_t AcLuminanceValues        [162]   =                                        // => 162 codes
    { 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08, // 16*10+2 symbols because
      0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28, // upper 4 bits can be 0..F
      0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59, // while lower 4 bits can be 1..A
      0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89, // plus two special codes 0x00 and 0xF0
      0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6, // order of these symbols was determined empirically by JPEG committee
      0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
      0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };
// Huffman definitions for second DC/AC tables (chrominance / Cb and Cr channels)
const uint8_t DcChrominanceCodesPerBitsize[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };   // sum = 12
const uint8_t DcChrominanceValues         [12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes (identical to DcLuminanceValues)
const uint8_t AcChrominanceCodesPerBitsize[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 }; // sum = 162
const uint8_t AcChrominanceValues        [162] =                                        // => 162 codes
    { 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91, // same number of symbol, just different order
      0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26, // (which is more efficient for AC coding)
      0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,
      0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
      0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,
      0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
      0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };
const int16_t CodeWordLimit = 2048; // +/-2^11, maximum value after DCT

// ////////////////////////////////////////
// structs

// represent a single Huffman code
struct BitCode
{
  BitCode() = default; // undefined state, must be initialized at a later time
  BitCode(uint16_t code_, uint8_t numBits_)
  : code(code_), numBits(numBits_) {}
  uint16_t code;       // JPEG's Huffman codes are limited to 16 bits
  uint8_t  numBits;    // number of valid bits
};

// wrapper for bit output operations
struct BitWriter
{
  // user-supplied callback that writes/stores one byte
  TooJpeg::WRITE_ONE_BYTE output;
  // initialize writer
  explicit BitWriter(TooJpeg::WRITE_ONE_BYTE output_) : output(output_) {}

  // store the most recently encoded bits that are not written yet
  struct BitBuffer
  {
    int32_t data    = 0; // actually only at most 24 bits are used
    uint8_t numBits = 0; // number of valid bits (the right-most bits)
  } buffer;

  // write Huffman bits stored in BitCode, keep excess bits in BitBuffer
  BitWriter& operator<<(const BitCode& data)
  {
    // append the new bits to those bits leftover from previous call(s)
    buffer.numBits += data.numBits;
    buffer.data   <<= data.numBits;
    buffer.data    |= data.code;

    // write all "full" bytes
    while (buffer.numBits >= 8)
    {
      // extract highest 8 bits
      buffer.numBits -= 8;
      auto oneByte = uint8_t(buffer.data >> buffer.numBits);
      output(oneByte);

      if (oneByte == 0xFF) // 0xFF has a special meaning for JPEGs (it's a block marker)
        output(0);         // therefore pad a zero to indicate "nope, this one ain't a marker, it's just a coincidence"

      // note: I don't clear those written bits, therefore buffer.bits may contain garbage in the high bits
      //       if you really want to "clean up" (e.g. for debugging purposes) then uncomment the following line
      //buffer.bits &= (1 << buffer.numBits) - 1;
    }
    return *this;
  }

  // write all non-yet-written bits, fill gaps with 1s (that's a strange JPEG thing)
  void flush()
  {
    // at most seven set bits needed to "fill" the last byte: 0x7F = binary 0111 1111
    *this << BitCode(0x7F, 7); // I should set buffer.numBits = 0 but since there are no single bits written after flush() I can safely ignore it
  }

  // NOTE: all the following BitWriter functions IGNORE the BitBuffer and write straight to output !
  // write a single byte
  BitWriter& operator<<(uint8_t oneByte)
  {
    output(oneByte);
    return *this;
  }

  // write an array of bytes
  template <typename T, int Size>
  BitWriter& operator<<(T (&manyBytes)[Size])
  {
    for (auto c : manyBytes)
      output(c);
    return *this;
  }

  // start a new JFIF block
  void addMarker(uint8_t id, uint16_t length)
  {
    output(0xFF); output(id);     // ID, always preceded by 0xFF
    output(uint8_t(length >> 8)); // length of the block (big-endian, includes the 2 length bytes as well)
    output(uint8_t(length & 0xFF));
  }
};

// ////////////////////////////////////////
// functions / templates

// same as std::min()
template <typename Number>
Number minimum(Number value, Number maximum)
{
  return value <= maximum ? value : maximum;
}

// restrict a value to the interval [minimum, maximum]
template <typename Number, typename Limit>
Number clamp(Number value, Limit minValue, Limit maxValue)
{
  if (value <= minValue) return minValue; // never smaller than the minimum
  if (value >= maxValue) return maxValue; // never bigger  than the maximum
  return value;                           // value was inside interval, keep it
}

// convert from RGB to YCbCr, constants are similar to ITU-R, see https://en.wikipedia.org/wiki/YCbCr#JPEG_conversion
float rgb2y (float r, float g, float b) { return +0.299f   * r +0.587f   * g +0.114f   * b; }
float rgb2cb(float r, float g, float b) { return -0.16874f * r -0.33126f * g +0.5f     * b; }
float rgb2cr(float r, float g, float b) { return +0.5f     * r -0.41869f * g -0.08131f * b; }

// forward DCT computation "in one dimension" (fast AAN algorithm by Arai, Agui and Nakajima: "A fast DCT-SQ scheme for images")
void DCT(float block[8*8], uint8_t stride) // stride must be 1 (=horizontal) or 8 (=vertical)
{
  const auto SqrtHalfSqrt = 1.306562965f; //    sqrt((2 + sqrt(2)) / 2) = cos(pi * 1 / 8) * sqrt(2)
  const auto InvSqrt      = 0.707106781f; // 1 / sqrt(2)                = cos(pi * 2 / 8)
  const auto HalfSqrtSqrt = 0.382683432f; //     sqrt(2 - sqrt(2)) / 2  = cos(pi * 3 / 8)
  const auto InvSqrtSqrt  = 0.541196100f; // 1 / sqrt(2 - sqrt(2))      = cos(pi * 3 / 8) * sqrt(2)

  // modify in-place
  auto& block0 = block[0         ];
  auto& block1 = block[1 * stride];
  auto& block2 = block[2 * stride];
  auto& block3 = block[3 * stride];
  auto& block4 = block[4 * stride];
  auto& block5 = block[5 * stride];
  auto& block6 = block[6 * stride];
  auto& block7 = block[7 * stride];

  // based on https://dev.w3.org/Amaya/libjpeg/jfdctflt.c , the original variable names can be found in my comments
  auto add07 = block0 + block7; auto sub07 = block0 - block7; // tmp0, tmp7
  auto add16 = block1 + block6; auto sub16 = block1 - block6; // tmp1, tmp6
  auto add25 = block2 + block5; auto sub25 = block2 - block5; // tmp2, tmp5
  auto add34 = block3 + block4; auto sub34 = block3 - block4; // tmp3, tmp4

  auto add0347 = add07 + add34; auto sub07_34 = add07 - add34; // tmp10, tmp13 ("even part" / "phase 2")
  auto add1256 = add16 + add25; auto sub16_25 = add16 - add25; // tmp11, tmp12

  block0 = add0347 + add1256; block4 = add0347 - add1256; // "phase 3"

  auto z1 = (sub16_25 + sub07_34) * InvSqrt; // all temporary z-variables kept their original names
  block2 = sub07_34 + z1; block6 = sub07_34 - z1; // "phase 5"

  auto sub23_45 = sub25 + sub34; // tmp10 ("odd part" / "phase 2")
  auto sub12_56 = sub16 + sub25; // tmp11
  auto sub01_67 = sub16 + sub07; // tmp12

  auto z5 = (sub23_45 - sub01_67) * HalfSqrtSqrt;
  auto z2 = sub23_45 * InvSqrtSqrt  + z5;
  auto z3 = sub12_56 * InvSqrt;
  auto z4 = sub01_67 * SqrtHalfSqrt + z5;
  auto z6 = sub07 + z3; // z11 ("phase 5")
  auto z7 = sub07 - z3; // z13
  block1 = z6 + z4; block7 = z6 - z4; // "phase 6"
  block5 = z7 + z2; block3 = z7 - z2;
}

// run DCT, quantize and write Huffman bit codes
int16_t encodeBlock(BitWriter& writer, float block[8][8], const float scaled[8*8], int16_t lastDC,
                    const BitCode huffmanDC[256], const BitCode huffmanAC[256], const BitCode* codewords)
{
  // "linearize" the 8x8 block, treat it as a flat array of 64 floats
  auto block64 = (float*) block;

  // DCT: rows
  for (auto offset = 0; offset < 8; offset++)
    DCT(block64 + offset*8, 1);
  // DCT: columns
  for (auto offset = 0; offset < 8; offset++)
    DCT(block64 + offset*1, 8);

  // scale
  for (auto i = 0; i < 8*8; i++)
    block64[i] *= scaled[i];

  // encode DC (the first coefficient is the "average color" of the 8x8 block)
  auto DC = int(block64[0] + (block64[0] >= 0 ? +0.5f : -0.5f)); // C++11's nearbyint() achieves a similar effect

  // quantize and zigzag the other 63 coefficients
  auto posNonZero = 0; // find last coefficient which is not zero (because trailing zeros are encoded differently)
  int16_t quantized[8*8];
  for (auto i = 1; i < 8*8; i++) // start at 1 because block64[0]=DC was already processed
  {
    auto value = block64[ZigZagInv[i]];
    // round to nearest integer
    quantized[i] = int(value + (value >= 0 ? +0.5f : -0.5f)); // C++11's nearbyint() achieves a similar effect
    // remember offset of last non-zero coefficient
    if (quantized[i] != 0)
      posNonZero = i;
  }

  // same "average color" as previous block ?
  auto diff = DC - lastDC;
  if (diff == 0)
    writer << huffmanDC[0x00];   // yes, write a special short symbol
  else
  {
    auto bits = codewords[diff]; // nope, encode the difference to previous block's average color
    writer << huffmanDC[bits.numBits] << bits;
  }

  // encode ACs (quantized[1..63])
  auto offset = 0; // upper 4 bits count the number of consecutive zeros
  for (auto i = 1; i <= posNonZero; i++) // quantized[0] was already written, skip all trailing zeros, too
  {
    // zeros are encoded in a special way
    while (quantized[i] == 0) // found another zero ?
    {
      offset    += 0x10; // add 1 to the upper 4 bits
      // split into blocks of at most 16 consecutive zeros
      if (offset > 0xF0) // remember, the counter is in the upper 4 bits, 0xF = 15
      {
        writer << huffmanAC[0xF0]; // 0xF0 is a special code for "16 zeros"
        offset = 0;
      }
      i++;
    }

    auto encoded = codewords[quantized[i]];
    // combine number of zeros with the number of bits of the next non-zero value
    writer << huffmanAC[offset + encoded.numBits] << encoded; // and the value itself
    offset = 0;
  }

  // send end-of-block code (0x00), only needed if there are trailing zeros
  if (posNonZero < 8*8 - 1) // = 63
    writer << huffmanAC[0x00];

  return DC;
}

// Jon's code includes the pre-generated Huffman codes
// I don't like these "magic constants" and compute them on my own :-)
void generateHuffmanTable(const uint8_t numCodes[16], const uint8_t* values, BitCode result[256])
{
  // process all bitsizes 1 thru 16, no JPEG Huffman code is allowed to exceed 16 bits
  auto huffmanCode = 0;
  for (auto numBits = 1; numBits <= 16; numBits++)
  {
    // ... and each code of these bitsizes
    for (auto i = 0; i < numCodes[numBits - 1]; i++) // note: numCodes array starts at zero, but smallest bitsize is 1
      result[*values++] = BitCode(huffmanCode++, numBits);

    // next Huffman code needs to be one bit wider
    huffmanCode <<= 1;
  }
}

} // end of anonymous namespace

// -------------------- externally visible code --------------------

namespace TooJpeg
{
// the only exported function ...
bool writeJpeg(WRITE_ONE_BYTE output, const void* pixels_, unsigned short width, unsigned short height,
               bool isRGB, unsigned char quality_, bool downsample, const char* comment)
{
  // reject invalid pointers
  if (output == nullptr || pixels_ == nullptr)
    return false;
  // check image format
  if (width == 0 || height == 0)
    return false;

  // number of components
  const auto numComponents = isRGB ? 3 : 1;
  // note: if there is just one component (=grayscale), then only luminance needs to be stored in the file
  //       thus everything related to chrominance need not to be written to the JPEG
  //       I still compute a few things, like quantization tables to avoid a complete code mess

  // grayscale images can't be downsampled (because there are no Cb + Cr channels)
  if (!isRGB)
    downsample = false;

  // wrapper for all output operations
  BitWriter bitWriter(output);

  // ////////////////////////////////////////
  // JFIF headers
  const uint8_t HeaderJfif[2+2+16] =
      { 0xFF,0xD8,         // SOI marker (start of image)
        0xFF,0xE0,         // JFIF APP0 tag
        0,16,              // length: 16 bytes (14 bytes payload + 2 bytes for this length field)
        'J','F','I','F',0, // JFIF identifier, zero-terminated
        1,1,               // JFIF version 1.1
        0,                 // no density units specified
        0,1,0,1,           // density: 1 pixel "per pixel" horizontally and vertically
        0,0 };             // no thumbnail (size 0 x 0)
  bitWriter << HeaderJfif;

  // ////////////////////////////////////////
  // comment (optional)
  if (comment != nullptr)
  {
    // look for zero terminator
    auto length = 0; // = strlen(comment);
    while (comment[length] != 0)
      length++;

    // write COM marker
    bitWriter.addMarker(0xFE, 2+length); // block size is number of bytes (without zero terminator) + 2 bytes for this length field
    // ... and write the comment itself
    for (auto i = 0; i < length; i++)
      bitWriter << comment[i];
  }

  // ////////////////////////////////////////
  // adjust quantization tables to desired quality

  // quality level must be in 1 ... 100
  auto quality = clamp<uint16_t>(quality_, 1, 100);
  // convert to an internal JPEG quality factor, formula taken from libjpeg
  quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

  uint8_t quantLuminance  [8*8];
  uint8_t quantChrominance[8*8];
  for (auto i = 0; i < 8*8; i++)
  {
    int luminance   = (DefaultQuantLuminance  [ZigZagInv[i]] * quality + 50) / 100;
    int chrominance = (DefaultQuantChrominance[ZigZagInv[i]] * quality + 50) / 100;

    // clamp to 1..255
    quantLuminance  [i] = clamp(luminance,   1, 255);
    quantChrominance[i] = clamp(chrominance, 1, 255);
  }

  // write quantization tables
  bitWriter.addMarker(0xDB, 2 + (isRGB ? 2 : 1) * (1 + 8*8)); // length: 65 bytes per table + 2 bytes for this length field
                                                              // each table has 64 entries and is preceded by an ID byte

  bitWriter   << 0x00 << quantLuminance;   // first  quantization table
  if (isRGB)
    bitWriter << 0x01 << quantChrominance; // second quantization table, only relevant for color images

  // ////////////////////////////////////////
  // write image infos (SOF0 - start of frame)
  bitWriter.addMarker(0xC0, 2+6+3*numComponents); // length: 6 bytes general info + 3 per channel + 2 bytes for this length field

  // 8 bits per channel
  bitWriter << 0x08
  // image dimensions (big-endian)
            << (height >> 8) << (height & 0xFF)
            << (width  >> 8) << (width  & 0xFF);

  // sampling and quantization tables for each component
  bitWriter << numComponents;       // 1 component (grayscale, Y only) or 3 components (Y,Cb,Cr)
  for (auto id = 1; id <= numComponents; id++)
    bitWriter <<  id                // component ID (Y=1, Cb=2, Cr=3)
    // bitmasks for sampling: highest 4 bits: horizontal, lowest 4 bits: vertical
              << (id == 1 && downsample ? 0x22 : 0x11) // 0x11 is default YCbCr 4:4:4 and 0x22 stands for YCbCr 4:2:0
              << (id == 1 ? 0 : 1); // use quantization table 0 for Y, table 1 for Cb and Cr

  // ////////////////////////////////////////
  // Huffman tables
  // DHT marker - define Huffman tables
  bitWriter.addMarker(0xC4, isRGB ? (2+208+208) : (2+208));
                            // 2 bytes for the length field, store chrominance only if needed
                            //   1+16+12  for the DC luminance
                            //   1+16+162 for the AC luminance   (208 = 1+16+12 + 1+16+162)
                            //   1+16+12  for the DC chrominance
                            //   1+16+162 for the AC chrominance (208 = 1+16+12 + 1+16+162, same as above)

  // store luminance's DC+AC Huffman table definitions
  bitWriter << 0x00 // highest 4 bits: 0 => DC, lowest 4 bits: 0 => Y (baseline)
            << DcLuminanceCodesPerBitsize
            << DcLuminanceValues;
  bitWriter << 0x10 // highest 4 bits: 1 => AC, lowest 4 bits: 0 => Y (baseline)
            << AcLuminanceCodesPerBitsize
            << AcLuminanceValues;

  // compute actual Huffman code tables (see Jon's code for precalculated tables)
  BitCode huffmanLuminanceDC[256];
  BitCode huffmanLuminanceAC[256];
  generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, huffmanLuminanceDC);
  generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, huffmanLuminanceAC);

  // chrominance is only relevant for color images
  BitCode huffmanChrominanceDC[256];
  BitCode huffmanChrominanceAC[256];
  if (isRGB)
  {
    // store luminance's DC+AC Huffman table definitions
    bitWriter << 0x01 // highest 4 bits: 0 => DC, lowest 4 bits: 1 => Cr,Cb (baseline)
              << DcChrominanceCodesPerBitsize
              << DcChrominanceValues;
    bitWriter << 0x11 // highest 4 bits: 1 => AC, lowest 4 bits: 1 => Cr,Cb (baseline)
              << AcChrominanceCodesPerBitsize
              << AcChrominanceValues;

    // compute actual Huffman code tables (see Jon's code for precalculated tables)
    generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, huffmanChrominanceDC);
    generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, huffmanChrominanceAC);
  }

  // ////////////////////////////////////////
  // start of scan (there is only a single scan for baseline JPEGs)
  bitWriter.addMarker(0xDA, 2+1+2*numComponents+3); // 2 bytes for the length field, 1 byte for number of components,
                                                    // then 2 bytes for each component and 3 bytes for spectral selection

  // assign Huffman tables to each component
  bitWriter << numComponents;
  for (auto id = 1; id <= numComponents; id++)
    // highest 4 bits: DC Huffman table, lowest 4 bits: AC Huffman table
    bitWriter << id << (id == 1 ? 0x00 : 0x11); // Y: tables 0 for DC and AC; Cb + Cr: tables 1 for DC and AC

  // constant values for our baseline JPEGs (which have a single sequential scan)
  static const uint8_t Spectral[3] = { 0, 63, 0 }; // spectral selection: must be from 0 to 63; successive approximation must be 0
  bitWriter << Spectral;

  // ////////////////////////////////////////
  // adjust quantization tables with AAN scaling factors to simplify DCT
  float scaledLuminance  [8*8];
  float scaledChrominance[8*8];
  for (auto i = 0; i < 8*8; i++)
  {
    auto row    = ZigZagInv[i] / 8; // same as ZigZagInv[i] >> 3
    auto column = ZigZagInv[i] % 8; // same as ZigZagInv[i] &  7

    // scaling constants for AAN DCT algorithm: AanScaleFactors[0] = 1, AanScaleFactors[k=1..7] = cos(k*PI/16) * sqrt(2)
    static const float AanScaleFactors[8] = { 1, 1.387039845f, 1.306562965f, 1.175875602f, 1, 0.785694958f, 0.541196100f, 0.275899379f };
    auto factor = 1 / (AanScaleFactors[row] * AanScaleFactors[column] * 8);
    scaledLuminance  [ZigZagInv[i]] = factor / quantLuminance  [i];
    scaledChrominance[ZigZagInv[i]] = factor / quantChrominance[i];
    // if you really want JPEGs that are bitwise identical to Jon Olick's code then you need slightly different formulas (note: sqrt(8) = 2.828427125f)
    //static const float aasf[] = { 1.0f * 2.828427125f, 1.387039845f * 2.828427125f, 1.306562965f * 2.828427125f, 1.175875602f * 2.828427125f, 1.0f * 2.828427125f, 0.785694958f * 2.828427125f, 0.541196100f * 2.828427125f, 0.275899379f * 2.828427125f }; // line 240 of jo_jpeg.cpp
    //scaledLuminance  [ZigZagInv[i]] = 1 / (quantLuminance  [i] * aasf[row] * aasf[column]); // lines 266-267 of jo_jpeg.cpp
    //scaledChrominance[ZigZagInv[i]] = 1 / (quantChrominance[i] * aasf[row] * aasf[column]);
  }

  // ////////////////////////////////////////
  // precompute JPEG codewords for quantized DCT
  BitCode  codewordsArray[2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
  BitCode* codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
  uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
  int32_t mask    = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
  for (int16_t value = 1; value < CodeWordLimit; value++)
  {
    // numBits = position of highest set bit (ignoring the sign)
    // mask    = (2^numBits) - 1
    if (value > mask) // one more bit ?
    {
      numBits++;
      mask = (mask << 1) | 1; // append a set bit
    }
    codewords[-value] = BitCode(mask - value, numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
    codewords[+value] = BitCode(       value, numBits);
  }

  // just convert image data from void*
  auto pixels = (const uint8_t*)pixels_;

  // the next two variables are frequently used when checking for image borders
  const auto maxWidth  = width  - 1; // "last row"
  const auto maxHeight = height - 1; // "bottom line"

  // process MCUs (minimum codes units) => image is subdivided into a grid of 8x8 or 16x16 tiles
  const auto sampling = downsample ? 2 : 1; // 1x1 or 2x2 sampling
  const auto mcuSize  = 8 * sampling;

  // average color of the previous MCU
  int16_t lastYDC = 0, lastCbDC = 0, lastCrDC = 0;
  // convert from RGB to YCbCr
  float Y[8][8], Cb[8][8], Cr[8][8];

  for (auto mcuY = 0; mcuY < height; mcuY += mcuSize) // each step is either 8 or 16 (=mcuSize)
    for (auto mcuX = 0; mcuX < width; mcuX += mcuSize)
    {
      // YCbCr 4:4:4 format: each MCU is a 8x8 block - the same applies to grayscale images, too
      // YCbCr 4:2:0 format: each MCU represents a 16x16 block, stored as 4x 8x8 Y-blocks plus 1x 8x8 Cb and 1x 8x8 Cr block)
      for (auto blockY = 0; blockY < mcuSize; blockY += 8) // iterate once (YCbCr444 and grayscale) or twice (YCbCr420)
        for (auto blockX = 0; blockX < mcuSize; blockX += 8)
        {
          // now we finally have an 8x8 block ...
          for (auto deltaY = 0; deltaY < 8; deltaY++)
          {
            auto column = minimum(mcuX + blockX         , maxWidth); // must not exceed image borders, replicate last row/column if needed
            auto row    = minimum(mcuY + blockY + deltaY, maxHeight);
            for (auto deltaX = 0; deltaX < 8; deltaX++)
            {
              // find actual pixel position within the current image
              auto pixelPos = row * int(width) + column; // the cast ensures that we don't run into multiplication overflows
              if (column < maxWidth)
                column++;

              // grayscale images have solely a Y channel which can be easily derived from the input pixel by shifting it by 128
              if (!isRGB)
              {
                Y[deltaY][deltaX] = pixels[pixelPos] - 128.f;
                continue;
              }

              // RGB: 3 bytes per pixel (whereas grayscale images have only 1 byte per pixel)
              auto r = pixels[3 * pixelPos    ];
              auto g = pixels[3 * pixelPos + 1];
              auto b = pixels[3 * pixelPos + 2];

              Y   [deltaY][deltaX] = rgb2y (r, g, b) - 128; // again, the JPEG standard requires Y to be shifted by 128
              // YCbCr444 is easy - the more complex YCbCr420 has to be computed about 20 lines below in a second pass
              if (!downsample)
              {
                Cb[deltaY][deltaX] = rgb2cb(r, g, b); // standard RGB-to-YCbCr conversion
                Cr[deltaY][deltaX] = rgb2cr(r, g, b);
              }
            }
          }

        // encode Y channel
        lastYDC = encodeBlock(bitWriter, Y, scaledLuminance, lastYDC, huffmanLuminanceDC, huffmanLuminanceAC, codewords);
        // Cb and Cr are encoded about 50 lines below
      }

      // grayscale images don't need any Cb and Cr information
      if (!isRGB)
        continue;

      // ////////////////////////////////////////
      // the following lines are only relevant for YCbCr420:
      // average/downsample chrominance of four pixels while respecting the image borders
      if (downsample)
        for (short deltaY = 7; downsample && deltaY >= 0; deltaY--) // iterating loop in reverse increases cache read efficiency
        {
          auto row      = minimum(mcuY + 2*deltaY, maxHeight); // each deltaX/Y step covers a 2x2 area
          auto column   =         mcuX;                        // column is updated inside next loop
          auto pixelPos = (row * int(width) + column) * 3;     // numComponents = 3

          // deltas (in bytes) to next row / column, must not exceed image borders
          auto rowStep    = (row    < maxHeight) ? 3 * int(width) : 0; // always numComponents*width except for bottom    line
          auto columnStep = (column < maxWidth ) ? 3              : 0; // always numComponents       except for rightmost pixel

          for (short deltaX = 0; deltaX < 8; deltaX++)
          {
            // let's add all four samples (2x2 area)
            auto right     = pixelPos + columnStep;
            auto down      = pixelPos +              rowStep;
            auto downRight = pixelPos + columnStep + rowStep;

            // note: cast from 8 bits to >8 bits to avoid overflows when adding
            auto r = short(pixels[pixelPos    ]) + pixels[right    ] + pixels[down    ] + pixels[downRight    ];
            auto g = short(pixels[pixelPos + 1]) + pixels[right + 1] + pixels[down + 1] + pixels[downRight + 1];
            auto b = short(pixels[pixelPos + 2]) + pixels[right + 2] + pixels[down + 2] + pixels[downRight + 2];

            // convert to Cb and Cr
            Cb[deltaY][deltaX] = rgb2cb(r, g, b) / 4; // I still have to divide r,g,b by 4 to get their average values
            Cr[deltaY][deltaX] = rgb2cr(r, g, b) / 4; // it's a bit faster if done AFTER CbCr conversion

            // step forward to next 2x2 area
            pixelPos += 2*3; // 2 pixels => 6 bytes (2*numComponents)
            column   += 2;

            // reached right border ?
            if (column >= maxWidth)
            {
              columnStep = 0;
              pixelPos = ((row + 1) * int(width) - 1) * 3; // same as (row * width + maxWidth) * numComponents => current's row last pixel
            }
          }
        } // end of YCbCr420 code for Cb and Cr

      // encode Cb and Cr
      lastCbDC = encodeBlock(bitWriter, Cb, scaledChrominance, lastCbDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
      lastCrDC = encodeBlock(bitWriter, Cr, scaledChrominance, lastCrDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
    }

  bitWriter.flush(); // now image is completely encoded, write any bits still left in the buffer

  // ///////////////////////////
  // EOI marker
  bitWriter << 0xFF << 0xD9; // this marker has no length, therefore I can't use addMarker()
  return true;
} // writeJpeg()
} // namespace TooJpeg
