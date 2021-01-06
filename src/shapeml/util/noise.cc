// Shape Modeling Language (ShapeML)
// Copyright (C) 2019 Stefan Lienhard
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "shapeml/util/noise.h"

namespace shapeml {

namespace util {

static const unsigned char permutations[512] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140,
    36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120,
    234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
    134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133,
    230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161,
    1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130,
    116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250,
    124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227,
    47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44,
    154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98,
    108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34,
    242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14,
    239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121,
    50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243,
    141, 128, 195, 78, 66, 215, 61, 156, 180,

    // Copy of above permuatation list to avoid a static initializer.
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140,
    36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120,
    234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
    134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133,
    230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161,
    1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130,
    116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250,
    124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227,
    47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44,
    154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98,
    108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34,
    242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14,
    239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121,
    50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243,
    141, 128, 195, 78, 66, 215, 61, 156, 180};

static float Fade(float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float Lerp(float a, float b, float t) { return a + t * (b - a); }

static int Floor(float f) {
  int fi = static_cast<int>(f);
  return (f < fi) ? fi - 1 : fi;
}

// Note that the overall range of the noise values depend on the length of the
// gradient vectors.
// With generic random gradients of uniform length, the range of the values is
// [-sqrt(n)/2, sqrt(n)/2]. See:
// https://github.com/rudi-c/perlin-range
// https://stackoverflow.com/questions/18261982/output-range-of-perlin-noise
// With a predefined set of gradients such as Perlin's original choice (that we
// also use for 3D noise), the range is a little bit larger than [-1, 1].

// Return dot product of (x, y) with gradient corresponding to hash.
static float Grad2(unsigned char hash, float x, float y) {
  switch (hash & 7) {
    case 0:
      return x + y;  // ( 1,  1)
    case 1:
      return -x + y;  // (-1,  1)
    case 2:
      return x - y;  // ( 1, -1)
    case 3:
      return -x - y;  // (-1, -1)
    case 4:
      return x;  // ( 1,  0)
    case 5:
      return -x;  // (-1,  0)
    case 6:
      return y;  // ( 0,  1)
    case 7:
      return -y;  // ( 0, -1)
    default:
      return 0.0f;  // Never happens.
  }
}

// Return dot product of (x, y, z) with gradient corresponding to hash.
// More readable than Perlin's original version code. Taken from:
// https://riven8192.blogspot.ch/2010/08/calculate-perlinnoise-twice-as-fast.html
static float Grad3(unsigned char hash, float x, float y, float z) {
  switch (hash & 15) {
    case 0:
      return x + y;  // ( 1,  1,  0)
    case 1:
      return -x + y;  // (-1,  1,  0)
    case 2:
      return x - y;  // ( 1, -1,  0)
    case 3:
      return -x - y;  // (-1, -1,  0)
    case 4:
      return x + z;  // ( 1,  0,  1)
    case 5:
      return -x + z;  // (-1,  0,  1)
    case 6:
      return x - z;  // ( 1,  0, -1)
    case 7:
      return -x - z;  // (-1,  0, -1)
    case 8:
      return y + z;  // ( 0,  1,  1)
    case 9:
      return -y + z;  // ( 0, -1,  1)
    case 10:
      return y - z;  // ( 0,  1, -1)
    case 11:
      return -y - z;  // ( 0, -1, -1)
    case 12:
      return y + x;  // ( 1,  1,  0)
    case 13:
      return -x + y;  // (-1,  1,  0)
    case 14:
      return -y + z;  // ( 0, -1,  1)
    case 15:
      return -y - z;  // ( 0, -1, -1)
    default:
      return 0.0f;  // Never happens.
  }
}

float PerlinNoise2(float x, float y) {
  int X = Floor(x);
  int Y = Floor(y);
  x -= X;
  y -= Y;

  X &= 255;
  Y &= 255;

  const float u = Fade(x);
  const float v = Fade(y);

  const unsigned char A = permutations[X] + Y;
  const unsigned char B = permutations[X + 1] + Y;

  const float grad_AA = Grad2(permutations[A], x, y);
  const float grad_BA = Grad2(permutations[B], x - 1.0f, y);
  const float grad_AB = Grad2(permutations[A + 1], x, y - 1.0f);
  const float grad_BB = Grad2(permutations[B + 1], x - 1.0f, y - 1.0f);

  return Lerp(Lerp(grad_AA, grad_BA, u), Lerp(grad_AB, grad_BB, u), v);
}

float PerlinNoise3(float x, float y, float z) {
  int X = Floor(x);
  int Y = Floor(y);
  int Z = Floor(z);
  x -= X;
  y -= Y;
  z -= Z;

  X &= 255;
  Y &= 255;
  Z &= 255;

  const float u = Fade(x);
  const float v = Fade(y);
  const float w = Fade(z);

  const unsigned char A = permutations[X] + Y;
  const unsigned char B = permutations[X + 1] + Y;

  const unsigned char AA = permutations[A] + Z;
  const unsigned char AB = permutations[A + 1] + Z;
  const unsigned char BA = permutations[B] + Z;
  const unsigned char BB = permutations[B + 1] + Z;

  const float grad_AAA = Grad3(permutations[AA], x, y, z);
  const float grad_BAA = Grad3(permutations[BA], x - 1.0f, y, z);
  const float grad_ABA = Grad3(permutations[AB], x, y - 1.0f, z);
  const float grad_BBA = Grad3(permutations[BB], x - 1.0f, y - 1.0f, z);
  const float grad_AAB = Grad3(permutations[AA + 1], x, y, z - 1.0f);
  const float grad_BAB = Grad3(permutations[BA + 1], x - 1.0f, y, z - 1.0f);
  const float grad_ABB = Grad3(permutations[AB + 1], x, y - 1.0f, z - 1.0f);
  const float grad_BBB =
      Grad3(permutations[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f);

  return Lerp(Lerp(Lerp(grad_AAA, grad_BAA, u), Lerp(grad_ABA, grad_BBA, u), v),
              Lerp(Lerp(grad_AAB, grad_BAB, u), Lerp(grad_ABB, grad_BBB, u), v),
              w);
}

// Fractal Brownian Motion
float fBm2(float x, float y, int octaves, float lacunarity, float gain) {
  float frequency = 1.0f;
  float amplitude = 1.0f;
  float sum = 0.0f;

  for (int i = 0; i < octaves; i++) {
    sum += PerlinNoise2(x * frequency, y * frequency) * amplitude;
    frequency *= lacunarity;
    amplitude *= gain;
  }
  return sum;
}

float fBm3(float x, float y, float z, int octaves, float lacunarity,
           float gain) {
  float frequency = 1.0f;
  float amplitude = 1.0f;
  float sum = 0.0f;

  for (int i = 0; i < octaves; i++) {
    sum +=
        PerlinNoise3(x * frequency, y * frequency, z * frequency) * amplitude;
    frequency *= lacunarity;
    amplitude *= gain;
  }
  return sum;
}

float fBm2(float x, float y, int octaves) {
  return fBm2(x, y, octaves, 2.0f, 0.5f);
}

float fBm3(float x, float y, float z, int octaves) {
  return fBm3(x, y, z, octaves, 2.0f, 0.5f);
}

}  // namespace util

}  // namespace shapeml
