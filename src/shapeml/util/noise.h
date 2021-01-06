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

#pragma once

namespace shapeml {

namespace util {

// This implements Perlin noise:
// http://mrl.nyu.edu/~perlin/noise/

float PerlinNoise2(float x, float y);
float PerlinNoise3(float x, float y, float z);

float fBm2(float x, float y, int octaves, float lacunarity, float gain);
float fBm3(float x, float y, float z, int octaves, float lacunarity,
           float gain);

// Sets lacunarity = 2.0 and gain = 0.5.
float fBm2(float x, float y, int octaves);
float fBm3(float x, float y, float z, int octaves);

// TODO(stefalie): Add turbulence and ridged multifractals.

}  // namespace util

}  // namespace shapeml
