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

#include "shapeml/geometry/primitives.h"

#include <cassert>
#include <iostream>

namespace shapeml {

namespace geometry {

void PolygonUnitSquare(Vec3Vec* polygon, Vec2Vec* uvs) {
  *polygon = {Vec3(-0.5, 0.0, 0.5), Vec3(0.5, 0.0, 0.5), Vec3(0.5, 0.0, -0.5),
              Vec3(-0.5, 0.0, -0.5)};

  if (uvs) {
    *uvs = {Vec2(0.0, 0.0), Vec2(1.0, 0.0), Vec2(1.0, 1.0), Vec2(0.0, 1.0)};
  }
}

void PolygonUnitCircle(unsigned n_phi, Vec3Vec* polygon, Vec2Vec* uvs) {
  const Scalar start_angle = 1.5 * M_PI;
  for (unsigned i = 0; i < n_phi; ++i) {
    const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
    polygon->push_back(Vec3(cos(a) * 0.5, 0.0, -sin(a) * 0.5));

    if (uvs) {
      uvs->push_back(
          Vec2(0.5 + polygon->back().x(), 0.5 - polygon->back().z()));
    }
  }
}

void PolygonL(Scalar w, Scalar h, Scalar back_width, Scalar left_width,
              Vec3Vec* polygon, Vec2Vec* uvs) {
  *polygon = {Vec3(0.0, 0.0, h),
              Vec3(left_width, 0.0, h),
              Vec3(left_width, 0.0, back_width),
              Vec3(w, 0.0, back_width),
              Vec3(w, 0.0, 0.0),
              Vec3(0.0, 0.0, 0.0)};

  if (uvs) {
    *uvs = {Vec2(0.0, 0.0),
            Vec2(left_width / w, 0.0),
            Vec2(left_width / w, (h - back_width) / h),
            Vec2(1.0, (h - back_width) / h),
            Vec2(1.0, 1.0),
            Vec2(0.0, 1.0)};
  }
}

void PolygonU(Scalar w, Scalar h, Scalar back_width, Scalar left_width,
              Scalar right_width, Vec3Vec* polygon, Vec2Vec* uvs) {
  *polygon = {Vec3(0.0, 0.0, h),
              Vec3(left_width, 0.0, h),
              Vec3(left_width, 0.0, back_width),
              Vec3(w - right_width, 0.0, back_width),
              Vec3(w - right_width, 0.0, h),
              Vec3(w, 0.0, h),
              Vec3(w, 0.0, 0.0),
              Vec3(0.0, 0.0, 0.0)};

  if (uvs) {
    *uvs = {Vec2(0.0, 0.0),
            Vec2(left_width / w, 0.0),
            Vec2(left_width / w, (h - back_width) / h),
            Vec2((w - right_width) / w, (h - back_width) / h),
            Vec2((w - right_width) / w, 0.0),
            Vec2(1.0, 0.0),
            Vec2(1.0, 1.0),
            Vec2(0.0, 1.0)};
  }
}

void PolygonT(Scalar w, Scalar h, Scalar top_width, Scalar vertical_width,
              Scalar vertical_position, Vec3Vec* polygon, Vec2Vec* uvs) {
  const Scalar w1 = vertical_position * (w - vertical_width);
  const Scalar w2 = w1 + vertical_width;

  *polygon = {Vec3(w1, 0.0, h),          Vec3(w2, 0.0, h),
              Vec3(w2, 0.0, top_width),  Vec3(w, 0.0, top_width),
              Vec3(w, 0.0, 0.0),         Vec3(0.0, 0.0, 0.0),
              Vec3(0.0, 0.0, top_width), Vec3(w1, 0.0, top_width)};

  if (uvs) {
    // It's becoming too boring to write it all out, let's loop.
    uvs->resize(polygon->size());
    for (size_t i = 0; i < polygon->size(); ++i) {
      const Vec3& p = polygon->at(i);
      uvs->at(i) = Vec2(p.x() / w, (h - p.z()) / h);
    }
  }
}

void PolygonH(Scalar w, Scalar h, Scalar left_width, Scalar horizontal_width,
              Scalar right_width, Scalar horizontal_position, Vec3Vec* polygon,
              Vec2Vec* uvs) {
  const Scalar h1 = horizontal_position * (h - horizontal_width);
  const Scalar h2 = h1 + horizontal_width;

  *polygon = {Vec3(0.0, 0.0, h),
              Vec3(left_width, 0.0, h),
              Vec3(left_width, 0.0, h2),
              Vec3(w - right_width, 0.0, h2),
              Vec3(w - right_width, 0.0, h),
              Vec3(w, 0.0, h),
              Vec3(w, 0.0, 0.0),
              Vec3(w - right_width, 0.0, 0.0),
              Vec3(w - right_width, 0.0, h1),
              Vec3(left_width, 0.0, h1),
              Vec3(left_width, 0.0, 0.0),
              Vec3(0.0, 0.0, 0.0)};

  if (uvs) {
    uvs->resize(polygon->size());
    for (size_t i = 0; i < polygon->size(); ++i) {
      const Vec3& p = polygon->at(i);
      uvs->at(i) = Vec2(p.x() / w, (h - p.z()) / h);
    }
  }
}

void MeshUnitGrid(unsigned n_x, unsigned n_z, Vec3Vec* vertices,
                  IdxVecVec* indices, Vec2Vec* uvs) {
  assert(n_x >= 1 && n_z >= 1 && vertices && indices);

  const Scalar step_x = 1.0 / n_x;
  const Scalar step_z = 1.0 / n_z;

  for (unsigned j = 0; j <= n_z; ++j) {
    for (unsigned i = 0; i <= n_x; ++i) {
      vertices->push_back(Vec3(i * step_x - 0.5, 0, j * step_z - 0.5));

      if (uvs) {
        uvs->push_back(Vec2(i * step_x, 1.0 - j * step_z));
      }
    }
  }

  for (unsigned i = 0; i < n_x; ++i) {
    for (unsigned j = 0; j < n_z; ++j) {
      indices->push_back(IdxVec());
      indices->back().push_back(i + (j + 1) * (n_x + 1));
      indices->back().push_back(i + 1 + (j + 1) * (n_x + 1));
      indices->back().push_back(i + 1 + j * (n_x + 1));
      indices->back().push_back(i + j * (n_x + 1));
    }
  }
}

void MeshUnitDisk(unsigned n_phi, unsigned n_rad, Vec3Vec* vertices,
                  IdxVecVec* indices, Vec2Vec* uvs) {
  const Scalar step_r = 1.0 / n_rad;
  const Scalar start_angle = 1.5 * M_PI;

  vertices->push_back(Vec3(0.0, 0.0, 0.0));
  if (uvs) {
    uvs->push_back(Vec2(0.5, 0.5));
  }

  for (unsigned j = 1; j <= n_rad; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar rad = j * step_r;
      vertices->push_back(Vec3(rad * cos(a) * 0.5, 0.0, rad * -sin(a) * 0.5));

      if (uvs) {
        uvs->push_back(
            Vec2(0.5 + vertices->back().x(), 0.5 - vertices->back().z()));
      }
    }
  }

  for (unsigned j = 0; j < n_rad; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      if (j == 0) {
        indices->push_back(IdxVec());
        indices->back().push_back(1 + i);
        indices->back().push_back(1 + (i + 1) % n_phi);
        indices->back().push_back(0);
      } else {
        indices->push_back(IdxVec());
        indices->back().push_back(1 + i + +j * n_phi);
        indices->back().push_back(1 + (i + 1) % n_phi + j * n_phi);
        indices->back().push_back(1 + (i + 1) % n_phi + (j - 1) * n_phi);
        indices->back().push_back(1 + i + (j - 1) * n_phi);
      }
    }
  }
}

void MeshUnitCube(Vec3Vec* vertices, IdxVecVec* indices, Vec2Vec* uvs,
                  IdxVecVec* uv_indices) {
  *vertices = {Vec3(-0.5, -0.5, 0.5), Vec3(0.5, -0.5, 0.5),
               Vec3(0.5, -0.5, -0.5), Vec3(-0.5, -0.5, -0.5),
               Vec3(-0.5, 0.5, 0.5),  Vec3(0.5, 0.5, 0.5),
               Vec3(0.5, 0.5, -0.5),  Vec3(-0.5, 0.5, -0.5)};

  *indices = {
      {4, 5, 6, 7},  // Top
      {3, 2, 1, 0},  // Bottom
      {0, 1, 5, 4},  // Front
      {1, 2, 6, 5},  // Right
      {2, 3, 7, 6},  // Back
      {3, 0, 4, 7}   // Left
  };

  if (uvs) {
    *uvs = {Vec2(0.0, 0.0), Vec2(1.0, 0.0), Vec2(1.0, 1.0), Vec2(0.0, 1.0)};

    // The uv indides are the same for each face.
    *uv_indices = IdxVecVec(6, {0, 1, 2, 3});
  }
}

void MeshUnitBox(unsigned n_x, unsigned n_y, unsigned n_z, Vec3Vec* vertices,
                 IdxVecVec* indices, Vec2Vec* uvs, IdxVecVec* uv_indices) {
  assert(n_x >= 1 && n_y >= 1 && n_z >= 1 && vertices && indices);

  const Scalar step_x = 1.0 / n_x;
  const Scalar step_y = 1.0 / n_y;
  const Scalar step_z = 1.0 / n_z;

  // The 4 sides
  for (unsigned j = 0; j <= n_y; ++j) {
    for (unsigned k = 0; k < n_z; ++k) {
      vertices->push_back(Vec3(-0.5, j * step_y - 0.5, k * step_z - 0.5));
    }

    for (unsigned i = 0; i < n_x; ++i) {
      vertices->push_back(Vec3(i * step_x - 0.5, j * step_y - 0.5, 0.5));
    }

    for (unsigned k = n_z; k > 0; --k) {
      vertices->push_back(Vec3(0.5, j * step_y - 0.5, k * step_z - 0.5));
    }

    for (unsigned i = n_x; i > 0; --i) {
      vertices->push_back(Vec3(i * step_x - 0.5, j * step_y - 0.5, -0.5));
    }
  }

  if (uvs) {
    // uv coordinates in zy-plane
    for (unsigned j = 0; j <= n_y; ++j) {
      for (unsigned k = 0; k <= n_z; ++k) {
        uvs->push_back(Vec2(0.0 + k * step_z, 0.0 + j * step_y));
      }
    }

    // uv coordinates in xy-plane
    for (unsigned j = 0; j <= n_y; ++j) {
      for (unsigned i = 0; i <= n_x; ++i) {
        uvs->push_back(Vec2(0.0 + i * step_x, 0.0 + j * step_y));
      }
    }

    // uv coordinates in xz-plane
    for (unsigned k = 0; k <= n_z; ++k) {
      for (unsigned i = 0; i <= n_x; ++i) {
        uvs->push_back(Vec2(0.0 + i * step_x, 0.0 + k * step_z));
      }
    }
  }

  // The 4 sides' indices
  const unsigned level_offset = 2 * (n_x + n_z);
  for (unsigned j = 0; j < n_y; ++j) {
    for (unsigned t = 0; t < level_offset; ++t) {
      unsigned t_inc = (t + 1) % level_offset;

      indices->push_back(IdxVec());
      indices->back().push_back(t + j * level_offset);
      indices->back().push_back(t_inc + j * level_offset);
      indices->back().push_back(t_inc + (j + 1) * level_offset);
      indices->back().push_back(t + (j + 1) * level_offset);

      if (uvs) {
        unsigned it;
        unsigned width_offset;
        if (t < n_z) {
          it = t;
          width_offset = n_z + 1;
        } else if (t < n_z + n_x) {
          it = t - n_z + (n_z + 1) * (n_y + 1);
          width_offset = n_x + 1;
        } else if (t < 2 * n_z + n_x) {
          it = t - n_z - n_x;
          width_offset = n_z + 1;
        } else {
          it = t - 2 * n_z - n_x + (n_z + 1) * (n_y + 1);
          width_offset = n_x + 1;
        }

        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(it + j * width_offset);
        uv_indices->back().push_back(it + 1 + j * width_offset);
        uv_indices->back().push_back(it + 1 + (j + 1) * width_offset);
        uv_indices->back().push_back(it + (j + 1) * width_offset);
      }
    }
  }

  // Top indices
  unsigned start_ind = (unsigned)vertices->size();
  IdxVec tmp_ind;
  for (int k = n_z; k >= 0; --k) {
    for (unsigned i = 0; i <= n_x; ++i) {
      if (i == 0) {
        tmp_ind.push_back(start_ind - level_offset + k);
      } else if ((unsigned)k == n_z) {
        tmp_ind.push_back(start_ind - level_offset + n_z + i);
      } else if (i == n_x) {
        tmp_ind.push_back(start_ind - n_x - k);
      } else if ((unsigned)k == 0) {
        tmp_ind.push_back(start_ind - i);
      } else {
        tmp_ind.push_back((unsigned)vertices->size());
        vertices->push_back(Vec3(i * step_x - 0.5, 0.5, k * step_z - 0.5));
      }
    }
  }
  for (unsigned k = 0; k < n_z; ++k) {
    for (unsigned i = 0; i < n_x; ++i) {
      indices->push_back(IdxVec());
      indices->back().push_back(tmp_ind[i + k * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + 1 + k * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + 1 + (k + 1) * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + (k + 1) * (n_x + 1)]);

      if (uvs) {
        const unsigned width_offset = n_x + 1;
        const unsigned xz_offset =
            (n_z + 1) * (n_y + 1) + (n_x + 1) * (n_y + 1);
        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(i + k * width_offset + xz_offset);
        uv_indices->back().push_back(i + 1 + k * width_offset + xz_offset);
        uv_indices->back().push_back(i + 1 + (k + 1) * width_offset +
                                     xz_offset);
        uv_indices->back().push_back(i + (k + 1) * width_offset + xz_offset);
      }
    }
  }

  // Bottom
  tmp_ind.clear();
  for (unsigned k = 0; k <= n_z; ++k) {
    for (unsigned i = 0; i <= n_x; ++i) {
      if (i == 0) {
        tmp_ind.push_back(k);
      } else if (k == n_z) {
        tmp_ind.push_back(n_z + i);
      } else if (i == n_x) {
        tmp_ind.push_back(level_offset - n_x - k);
      } else if (k == 0) {
        tmp_ind.push_back(level_offset - i);
      } else {
        tmp_ind.push_back((unsigned)vertices->size());
        vertices->push_back(Vec3(i * step_x - 0.5, -0.5, k * step_z - 0.5));
      }
    }
  }
  for (unsigned k = 0; k < n_z; ++k) {
    for (unsigned i = 0; i < n_x; ++i) {
      indices->push_back(IdxVec());
      indices->back().push_back(tmp_ind[i + k * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + 1 + k * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + 1 + (k + 1) * (n_x + 1)]);
      indices->back().push_back(tmp_ind[i + (k + 1) * (n_x + 1)]);

      if (uvs) {
        const unsigned width_offset = n_x + 1;
        const unsigned xz_offset =
            (n_z + 1) * (n_y + 1) + (n_x + 1) * (n_y + 1);
        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(i + k * width_offset + xz_offset);
        uv_indices->back().push_back(i + 1 + k * width_offset + xz_offset);
        uv_indices->back().push_back(i + 1 + (k + 1) * width_offset +
                                     xz_offset);
        uv_indices->back().push_back(i + (k + 1) * width_offset + xz_offset);
      }
    }
  }
}

void MeshUnitCylinder(unsigned n_phi, unsigned n_y, unsigned n_rad,
                      Vec3Vec* vertices, IdxVecVec* indices, Vec3Vec* normals,
                      IdxVecVec* normal_indices, Vec2Vec* uvs,
                      IdxVecVec* uv_indices) {
  assert(n_phi >= 3 && n_y >= 1 && n_rad >= 1 && vertices && indices);

  const Scalar step_y = 1.0 / n_y;
  const Scalar step_r = 1.0 / n_rad;

  const Scalar start_angle = 1.5 * M_PI;
  for (unsigned j = 0; j <= n_y; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      vertices->push_back(Vec3(cos(a) * 0.5, j * step_y - 0.5, -sin(a) * 0.5));
    }
  }
  if (normals) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      normals->push_back(Vec3(cos(a), 0.0, -sin(a)));
    }
  }
  if (uvs) {
    for (unsigned j = 0; j <= n_y; ++j) {
      for (unsigned i = 0; i <= n_phi; ++i) {
        uvs->push_back(Vec2((Scalar)i / n_phi, j * step_y));
      }
    }
  }

  // Top
  const unsigned offset_top = (unsigned)vertices->size();
  vertices->push_back(Vec3(0.0, 0.5, 0.0));
  for (unsigned k = 1; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar rad = k * step_r;
      vertices->push_back(Vec3(rad * cos(a) * 0.5, 0.5, -rad * sin(a) * 0.5));
    }
  }

  if (normals) {
    normals->push_back(Vec3::UnitY());
  }

  unsigned offset_uv = uvs ? (unsigned)uvs->size() : 0;
  if (uvs) {
    uvs->push_back(Vec2(0.5, 0.5));
    for (unsigned k = 1; k <= n_rad; ++k) {
      for (unsigned i = 0; i < n_phi; ++i) {
        const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
        const Scalar rad = k * step_r;
        uvs->push_back(
            Vec2(rad * cos(a) * 0.5 + 0.5, 0.5 + rad * sin(a) * 0.5));
      }
    }
  }

  for (unsigned k = 0; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      if (k == 0 && n_rad == 1) {
        indices->push_back(IdxVec());
        indices->back().push_back(n_phi * n_y + i);
        indices->back().push_back(n_phi * n_y + (i + 1) % n_phi);
        indices->back().push_back(offset_top);
      } else if (k == 0) {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_top + 1 + i);
        indices->back().push_back(offset_top + 1 + (i + 1) % n_phi);
        indices->back().push_back(offset_top);
      } else if (k == n_rad - 1) {
        indices->push_back(IdxVec());
        indices->back().push_back(n_phi * n_y + i);
        indices->back().push_back(n_phi * n_y + (i + 1) % n_phi);
        indices->back().push_back(offset_top + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
        indices->back().push_back(offset_top + 1 + i + (k - 1) * n_phi);
      } else {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_top + 1 + i + +k * n_phi);
        indices->back().push_back(offset_top + 1 + (i + 1) % n_phi + k * n_phi);
        indices->back().push_back(offset_top + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
        indices->back().push_back(offset_top + 1 + i + (k - 1) * n_phi);
      }

      if (normals) {
        if (k == 0) {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 3,
                                        (unsigned)normals->size() - 1);
        } else {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 4,
                                        (unsigned)normals->size() - 1);
        }
      }

      if (uvs) {
        if (k == 0) {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + i);
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi);
          uv_indices->back().push_back(offset_uv);
        } else {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + i + k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       (k - 1) * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i + (k - 1) * n_phi);
        }
      }
    }
  }

  // Bottom
  const unsigned offset_bottom = (unsigned)vertices->size();
  vertices->push_back(Vec3(0.0, -0.5, 0.0));
  for (unsigned k = 1; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar rad = k * step_r;
      vertices->push_back(Vec3(rad * cos(a) * 0.5, -0.5, -rad * sin(a) * 0.5));
    }
  }

  if (normals) {
    normals->push_back(-Vec3::UnitY());
  }

  offset_uv = uvs ? (unsigned)uvs->size() : 0;
  if (uvs) {
    uvs->push_back(Vec2(0.5, 0.5));
    for (unsigned k = 1; k <= n_rad; ++k) {
      for (unsigned i = 0; i < n_phi; ++i) {
        const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
        const Scalar rad = k * step_r;
        uvs->push_back(
            Vec2(rad * cos(a) * 0.5 + 0.5, 0.5 - rad * sin(a) * 0.5));
      }
    }
  }

  for (unsigned k = 0; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      if (k == 0 && n_rad == 1) {
        indices->push_back(IdxVec());
        indices->back().push_back((i + 1) % n_phi);
        indices->back().push_back(i);
        indices->back().push_back(offset_bottom);
      } else if (k == 0) {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi);
        indices->back().push_back(offset_bottom + 1 + i);
        indices->back().push_back(offset_bottom);
      } else if (k == n_rad - 1) {
        indices->push_back(IdxVec());
        indices->back().push_back((i + 1) % n_phi);
        indices->back().push_back(i);
        indices->back().push_back(offset_bottom + 1 + i + (k - 1) * n_phi);
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
      } else {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  k * n_phi);
        indices->back().push_back(offset_bottom + 1 + i + +k * n_phi);
        indices->back().push_back(offset_bottom + 1 + i + (k - 1) * n_phi);
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
      }

      if (normals) {
        if (k == 0) {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 3,
                                        (unsigned)normals->size() - 1);
        } else {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 4,
                                        (unsigned)normals->size() - 1);
        }
      }

      if (uvs) {
        if (k == 0) {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i);
          uv_indices->back().push_back(offset_uv);
        } else {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i + k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i + (k - 1) * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       (k - 1) * n_phi);
        }
      }
    }
  }

  // Sides
  for (unsigned j = 0; j < n_y; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      indices->push_back(IdxVec());
      indices->back().push_back(i + n_phi * j);
      indices->back().push_back((i + 1) % n_phi + n_phi * j);
      indices->back().push_back((i + 1) % n_phi + n_phi * (j + 1));
      indices->back().push_back(i + n_phi * (j + 1));

      if (normals) {
        normal_indices->push_back(IdxVec());
        normal_indices->back().push_back(i);
        normal_indices->back().push_back((i + 1) % n_phi);
        normal_indices->back().push_back((i + 1) % n_phi);
        normal_indices->back().push_back(i);
      }

      if (uvs) {
        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(i + (n_phi + 1) * j);
        uv_indices->back().push_back((i + 1) + (n_phi + 1) * j);
        uv_indices->back().push_back((i + 1) + (n_phi + 1) * (j + 1));
        uv_indices->back().push_back(i + (n_phi + 1) * (j + 1));
      }
    }
  }
}

void MeshUnitCone(unsigned n_phi, unsigned n_y, unsigned n_rad,
                  Vec3Vec* vertices, IdxVecVec* indices, Vec3Vec* normals,
                  IdxVecVec* normal_indices, Vec2Vec* uvs,
                  IdxVecVec* uv_indices) {
  assert(n_phi >= 3 && n_y >= 1 && n_rad >= 1 && vertices && indices);

  const Scalar step_y = 1.0 / n_y;
  const Scalar step_r = 1.0 / n_rad;

  const Scalar start_angle = 1.5 * M_PI;
  for (unsigned j = 0; j < n_y; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar rad = 0.5 * (1.0 - j * step_y);
      vertices->push_back(Vec3(cos(a) * rad, j * step_y - 0.5, -sin(a) * rad));
    }
  }
  vertices->push_back(Vec3(0.0, 0.5, 0.0));

  if (normals) {
    const Scalar sqrt5_inv = 1.0 / sqrt(5.0);
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      normals->push_back(
          Vec3(cos(a) * 2.0 * sqrt5_inv, sqrt5_inv, -sin(a) * 2.0 * sqrt5_inv));
    }
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * i / n_phi;
      normals->push_back(
          Vec3(cos(a) * 2.0 * sqrt5_inv, sqrt5_inv, -sin(a) * 2.0 * sqrt5_inv));
    }
  }

  if (uvs) {
    for (unsigned j = 0; j < n_y; ++j) {
      const Scalar height = j * step_y;
      uvs->push_back(Vec2(0.5 * height, height));
      uvs->push_back(Vec2(1.0 - 0.5 * height, height));
    }
    uvs->push_back(Vec2(0.5, 1.0));
  }

  // Bottom
  const unsigned offset_bottom = (unsigned)vertices->size();
  vertices->push_back(Vec3(0.0, -0.5, 0.0));
  for (unsigned k = 1; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar rad = k * step_r;
      vertices->push_back(Vec3(rad * cos(a) * 0.5, -0.5, -rad * sin(a) * 0.5));
    }
  }

  if (normals) {
    normals->push_back(-Vec3::UnitY());
  }

  const unsigned offset_uv = (unsigned)uvs->size();
  if (uvs) {
    uvs->push_back(Vec2(0.5, 0.5));
    for (unsigned k = 1; k <= n_rad; ++k) {
      for (unsigned i = 0; i < n_phi; ++i) {
        const Scalar a = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
        const Scalar rad = k * step_r;
        uvs->push_back(
            Vec2(rad * cos(a) * 0.5 + 0.5, 0.5 - rad * sin(a) * 0.5));
      }
    }
  }

  for (unsigned k = 0; k < n_rad; ++k) {
    for (unsigned i = 0; i < n_phi; ++i) {
      if (k == 0 && n_rad == 1) {
        indices->push_back(IdxVec());
        indices->back().push_back((i + 1) % n_phi);
        indices->back().push_back(i);
        indices->back().push_back(offset_bottom);
      } else if (k == 0) {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi);
        indices->back().push_back(offset_bottom + 1 + i);
        indices->back().push_back(offset_bottom);
      } else if (k == n_rad - 1) {
        indices->push_back(IdxVec());
        indices->back().push_back((i + 1) % n_phi);
        indices->back().push_back(i);
        indices->back().push_back(offset_bottom + 1 + i + (k - 1) * n_phi);
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
      } else {
        indices->push_back(IdxVec());
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  k * n_phi);
        indices->back().push_back(offset_bottom + 1 + i + +k * n_phi);
        indices->back().push_back(offset_bottom + 1 + i + (k - 1) * n_phi);
        indices->back().push_back(offset_bottom + 1 + (i + 1) % n_phi +
                                  (k - 1) * n_phi);
      }

      if (normals) {
        if (k == 0) {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 3,
                                        (unsigned)normals->size() - 1);
        } else {
          normal_indices->push_back(IdxVec());
          normal_indices->back().insert(normal_indices->back().end(), 4,
                                        (unsigned)normals->size() - 1);
        }
      }

      if (uvs) {
        if (k == 0) {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i);
          uv_indices->back().push_back(offset_uv);
        } else {
          uv_indices->push_back(IdxVec());
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i + k * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + i + (k - 1) * n_phi);
          uv_indices->back().push_back(offset_uv + 1 + (i + 1) % n_phi +
                                       (k - 1) * n_phi);
        }
      }
    }
  }

  // Sides
  for (unsigned j = 0; j < n_y - 1; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      indices->push_back(IdxVec());
      indices->back().push_back(i + n_phi * j);
      indices->back().push_back((i + 1) % n_phi + n_phi * j);
      indices->back().push_back((i + 1) % n_phi + n_phi * (j + 1));
      indices->back().push_back(i + n_phi * (j + 1));

      if (normals) {
        normal_indices->push_back(IdxVec());
        normal_indices->back().push_back(i);
        normal_indices->back().push_back((i + 1) % n_phi);
        normal_indices->back().push_back((i + 1) % n_phi);
        normal_indices->back().push_back(i);
      }

      if (uvs) {
        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(j * 2);
        uv_indices->back().push_back(1 + j * 2);
        uv_indices->back().push_back(1 + (j + 1) * 2);
        uv_indices->back().push_back((j + 1) * 2);
      }
    }
  }

  for (unsigned i = 0; i < n_phi; ++i) {
    indices->push_back(IdxVec());
    indices->back().push_back(i + n_phi * (n_y - 1));
    indices->back().push_back((i + 1) % n_phi + n_phi * (n_y - 1));
    indices->back().push_back(n_phi * n_y);

    if (normals) {
      normal_indices->push_back(IdxVec());
      normal_indices->back().push_back(i);
      normal_indices->back().push_back((i + 1) % n_phi);
      normal_indices->back().push_back(i + n_phi);
    }

    if (uvs) {
      uv_indices->push_back(IdxVec());
      uv_indices->back().push_back((n_y - 1) * 2);
      uv_indices->back().push_back(1 + (n_y - 1) * 2);
      uv_indices->back().push_back(n_y * 2);
    }
  }
}

void MeshUnitSphere(unsigned n_phi, unsigned n_theta, Vec3Vec* vertices,
                    IdxVecVec* indices, Vec3Vec* normals,
                    IdxVecVec* normal_indices, Vec2Vec* uvs,
                    IdxVecVec* uv_indices) {
  assert(n_phi >= 3 && n_theta >= 2 && vertices && indices);

  const Scalar start_angle = 1.5 * M_PI;

  for (unsigned j = 1; j < n_theta; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      const Scalar phi = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
      const Scalar theta = M_PI * (-0.5 + (Scalar)j / n_theta);
      Vec3 tmp(cos(phi) * cos(theta), sin(theta), -sin(phi) * cos(theta));

      if (normals) {
        normals->push_back(tmp);
      }

      vertices->push_back(tmp * 0.5 + Vec3(0.5, 0.5, 0.5));
    }
  }

  vertices->push_back(Vec3(0.5, 0.0, 0.5));
  vertices->push_back(Vec3(0.5, 1.0, 0.5));

  if (normals) {
    normals->push_back(-Vec3::UnitY());
    normals->push_back(Vec3::UnitY());
  }

  if (uvs) {
    uvs->push_back(Vec2(0.0, 0.0));
    uvs->push_back(Vec2(1.0, 0.0));
    uvs->push_back(Vec2(0.5, 1.0));
    uvs->push_back(Vec2(0.5, 0.0));
    uvs->push_back(Vec2(1.0, 1.0));
    uvs->push_back(Vec2(0.0, 1.0));

    Scalar prev_rad = cos(M_PI * (-0.5 + 1.0 / n_theta));
    for (unsigned j = 2; j < n_theta; ++j) {
      const Scalar theta = M_PI * (-0.5 + (Scalar)j / n_theta);
      const Scalar rad = cos(theta);

      if (j <= n_theta / 2) {
        const Scalar offset = 0.5 * (1.0 - prev_rad / rad);
        uvs->push_back(Vec2(offset, 0.0));
        uvs->push_back(Vec2(1 - offset, 0.0));
      } else {
        const Scalar offset = 0.5 * (1.0 - rad / prev_rad);
        uvs->push_back(Vec2(offset, 1.0));
        uvs->push_back(Vec2(1 - offset, 1.0));
      }

      prev_rad = rad;
    }
  }

  // Top ring
  for (unsigned i = 0; i < n_phi; ++i) {
    indices->push_back(IdxVec());
    indices->back().push_back(n_phi * (n_theta - 2) + i);
    indices->back().push_back(n_phi * (n_theta - 2) + (i + 1) % n_phi);
    indices->back().push_back((unsigned)vertices->size() - 1);

    if (normals) {
      normal_indices->push_back(IdxVec());
      normal_indices->back().push_back(n_phi * (n_theta - 2) + i);
      normal_indices->back().push_back(n_phi * (n_theta - 2) + (i + 1) % n_phi);
      normal_indices->back().push_back((unsigned)vertices->size() - 1);
    }

    if (uvs) {
      uv_indices->push_back(IdxVec());
      uv_indices->back().push_back(0);
      uv_indices->back().push_back(1);
      uv_indices->back().push_back(2);
    }
  }

  // Everything in-between
  for (unsigned j = 1; j < n_theta - 1; ++j) {
    for (unsigned i = 0; i < n_phi; ++i) {
      indices->push_back(IdxVec());
      indices->back().push_back(i + (j - 1) * n_phi);
      indices->back().push_back((i + 1) % n_phi + (j - 1) * n_phi);
      indices->back().push_back((i + 1) % n_phi + j * n_phi);
      indices->back().push_back(i + j * n_phi);

      if (normals) {
        normal_indices->push_back(IdxVec());
        normal_indices->back().push_back(i + (j - 1) * n_phi);
        normal_indices->back().push_back((i + 1) % n_phi + (j - 1) * n_phi);
        normal_indices->back().push_back((i + 1) % n_phi + j * n_phi);
        normal_indices->back().push_back(i + j * n_phi);
      }
    }

    if (uvs) {
      IdxVec tmp(4);
      if (j + 1 <= n_theta / 2) {
        tmp[0] = 6 + (j - 1) * 2;
        tmp[1] = 7 + (j - 1) * 2;
        tmp[2] = 4;
        tmp[3] = 5;
      } else {
        tmp[0] = 0;
        tmp[1] = 1;
        tmp[2] = 7 + (j - 1) * 2;
        tmp[3] = 6 + (j - 1) * 2;
      }
      uv_indices->insert(uv_indices->end(), n_phi, tmp);
    }
  }

  // Bottom ring
  for (unsigned i = 0; i < n_phi; ++i) {
    indices->push_back(IdxVec());
    indices->back().push_back((unsigned)vertices->size() - 2);
    indices->back().push_back((i + 1) % n_phi);
    indices->back().push_back(i);

    if (normals) {
      normal_indices->push_back(IdxVec());
      normal_indices->back().push_back((unsigned)vertices->size() - 2);
      normal_indices->back().push_back((i + 1) % n_phi);
      normal_indices->back().push_back(i);
    }
  }

  if (uvs) {
    IdxVec tmp(3);
    tmp[0] = 3;
    tmp[1] = 4;
    tmp[2] = 5;
    uv_indices->insert(uv_indices->end(), n_phi, tmp);
  }
}

void MeshTorus(Scalar radius_1, Scalar radius_2, unsigned n_phi,
               unsigned n_theta, Vec3Vec* vertices, IdxVecVec* indices,
               Vec3Vec* normals, Vec2Vec* uvs, IdxVecVec* uv_indices) {
  assert(n_phi >= 3 && n_theta >= 3 && radius_2 > 0.0 && radius_1 >= radius_2 &&
         vertices && indices);

  const Scalar start_angle = 1.5 * M_PI;
  for (unsigned i = 0; i < n_phi; ++i) {
    const Scalar phi = start_angle + 2.0 * M_PI * (i - 0.5) / n_phi;
    for (unsigned j = 0; j < n_theta; ++j) {
      const Scalar theta = 2.0 * M_PI * (j - 0.5) / n_theta;
      const Scalar R_r_cost = radius_1 + radius_2 * cos(theta);
      vertices->push_back(Vec3(R_r_cost * cos(phi), radius_2 * sin(theta),
                               R_r_cost * -sin(phi)));

      if (normals) {
        normals->push_back(Vec3(cos(phi), sin(theta), -sin(phi)));
      }
    }
  }

  if (uvs) {
    for (unsigned i = 0; i <= n_phi; ++i) {
      for (unsigned j = 0; j <= n_theta; ++j) {
        uvs->push_back(Vec2((Scalar)i / n_phi, (j - 0.5) / n_theta));
      }
    }
  }

  // Face indices
  for (unsigned i = 0; i < n_phi; ++i) {
    for (unsigned j = 0; j < n_theta; ++j) {
      indices->push_back(IdxVec());
      indices->back().push_back(j + n_theta * i);
      indices->back().push_back(j + n_theta * ((i + 1) % n_phi));
      indices->back().push_back((j + 1) % n_theta +
                                n_theta * ((i + 1) % n_phi));
      indices->back().push_back((j + 1) % n_theta + n_theta * i);

      if (uvs) {
        uv_indices->push_back(IdxVec());
        uv_indices->back().push_back(j + (n_theta + 1) * i);
        uv_indices->back().push_back(j + (n_theta + 1) * (i + 1));
        uv_indices->back().push_back((j + 1) + (n_theta + 1) * (i + 1));
        uv_indices->back().push_back((j + 1) + (n_theta + 1) * i);
      }
    }
  }
}

}  // namespace geometry

}  // namespace shapeml
