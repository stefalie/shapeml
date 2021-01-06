// Shape Modeling Language (ShapeML)
// Copyright (C) 2018  Stefan Lienhard
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

#ifdef SKELETON_TEST_STANDALONE
#include <GLFW/glfw3.h>

#include <fstream>

#include "shapeml/geometry/skeleton/roof.h"
#else
#include <gtest/gtest.h>
#endif
#include "shapeml/geometry/skeleton/skeleton.h"

using shapeml::IdxVec;
using shapeml::IdxVecVec;
using shapeml::Vec2;
using shapeml::Vec2Vec;
using shapeml::Vec3;
using shapeml::Vec3Vec;
using shapeml::geometry::skeleton::Skeleton;

std::vector<Vec2Vec> polys;
std::vector<Skeleton*> skeletons;
std::vector<size_t> skeleton_sizes;

void CreateSkeletons() {
  // Convex test cases

  Vec2Vec verts, hole;

  verts = {
      Vec2(1.0, 1.0),   Vec2(20.0, 5.0), Vec2(30.0, 25.0),
      Vec2(20.0, 30.0), Vec2(1.0, 10.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(7);

  verts = {
      Vec2(30.0, 1.0), Vec2(50.0, 1.0),  Vec2(70.0, 1.0),
      Vec2(90.0, 1.0), Vec2(40.0, 40.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(3);

  verts = {
      Vec2(10.0, 60.0), Vec2(15.0, 55.0), Vec2(30.0, 50.0),
      Vec2(30.0, 70.0), Vec2(15.0, 65.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(6);

  verts = {
      Vec2(0.0, 35.0),
      Vec2(30.0, 35.0),
      Vec2(30.0, 48.0),
      Vec2(0.0, 48.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(5);

  verts = {
      Vec2(0.0, 80.0),  Vec2(10.0, 70.0),  Vec2(20.0, 70.0),  Vec2(30.0, 80.0),
      Vec2(30.0, 90.0), Vec2(20.0, 100.0), Vec2(10.0, 100.0), Vec2(0.0, 90.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(12);

  // Regular n-gon
  verts.clear();
  const int n = 5;
  for (int i = 0; i < n; ++i) {
    const double angle = static_cast<double>(i) / n * 2.0 * M_PI;
    verts.push_back(Vec2(50.0 + 15.0 * cos(angle), 50.0 + 15.0 * sin(angle)));
  }
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(n);

  // Concave test cases

  verts = {
      Vec2(90.0, 0.0),   Vec2(110.0, 0.0), Vec2(115.0, 20.0),
      Vec2(100.0, 10.0), Vec2(85.0, 20.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(7);

  verts = {
      Vec2(70.0, 20.0), Vec2(100.0, 20.0), Vec2(95.0, 30.0),
      Vec2(85.0, 30.0), Vec2(80.0, 45.0),  Vec2(70.0, 40.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(9);

  verts = {
      Vec2(100.0, 20.0), Vec2(150.0, 20.0), Vec2(140.0, 60.0),
      Vec2(125.0, 30.0), Vec2(110.0, 60.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(7);

  // L
  verts = {
      Vec2(70.0, 50.0), Vec2(100.0, 50.0), Vec2(100.0, 60.0),
      Vec2(80.0, 60.0), Vec2(80.0, 80.0),  Vec2(70.0, 80.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(8);

  // L with two widths
  verts = {
      Vec2(90.0, 70.0),  Vec2(130.0, 70.0),  Vec2(130.0, 80.0),
      Vec2(110.0, 80.0), Vec2(110.0, 100.0), Vec2(90.0, 100.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(9);

  // Cross
  verts = {
      Vec2(50.0, 70.0),  Vec2(60.0, 70.0),  Vec2(60.0, 90.0),
      Vec2(80.0, 90.0),  Vec2(80.0, 100.0), Vec2(60.0, 100.0),
      Vec2(60.0, 120.0), Vec2(50.0, 120.0), Vec2(50.0, 100.0),
      Vec2(30.0, 100.0), Vec2(30.0, 90.0),  Vec2(50.0, 90.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(16);

  // Wind rose
  verts = {
      Vec2(45.0, 110.0), Vec2(50.0, 125.0), Vec2(65.0, 130.0),
      Vec2(50.0, 135.0), Vec2(45.0, 150.0), Vec2(40.0, 135.0),
      Vec2(25.0, 130.0), Vec2(40.0, 125.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(8);

  // Fig. 3 in:
  // Felkel, Obdrzalek, 1998, Straight Skeleton Implementation
  verts = {
      Vec2(65.0, 105.0), Vec2(100.0, 105.0), Vec2(100.0, 127.5),
      Vec2(95.0, 117.5), Vec2(85.0, 127.5),  Vec2(75.0, 117.5),
      Vec2(65.0, 127.5),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(11);

  // Fig. 5 in:
  // Felkel, Obdrzalek, 1998, Straight Skeleton Implementation
  verts = {
      Vec2(135.0, 60.0), Vec2(195.0, 60.0), Vec2(187.5, 85.0),
      Vec2(180.0, 67.5), Vec2(172.5, 85.0), Vec2(165.0, 65.0),
      Vec2(157.5, 85.0), Vec2(150.0, 67.5), Vec2(142.5, 85.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(15);

  // V
  verts = {
      Vec2(190.0, 0.0),
      Vec2(200.0, 25.0),
      Vec2(190.0, 15.0),
      Vec2(180.0, 25.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(4);

  // V skewed
  verts = {
      Vec2(170.0, 0.0),
      Vec2(180.0, 25.0),
      Vec2(165.0, 15.0),
      Vec2(155.0, 25.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(5);

  // V skewed 2
  verts = {
      Vec2(190.0, 85.0),
      Vec2(200.0, 110.0),
      Vec2(192.0, 100.0),
      Vec2(175.0, 110.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(5);

  // Z
  verts = {
      Vec2(155.0, 25.0), Vec2(190.0, 25.0), Vec2(195.0, 30.0),
      Vec2(170.0, 35.0), Vec2(195.0, 55.0), Vec2(185.0, 60.0),
      Vec2(158.0, 60.0), Vec2(155.0, 53.0), Vec2(182.0, 49.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(15);

  verts = {
      Vec2(0.0, 180.0),  Vec2(26.0, 180.0), Vec2(2.0, 175.0), Vec2(30.0, 175.0),
      Vec2(30.0, 200.0), Vec2(15.0, 190.0), Vec2(5.0, 200.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(11);

  verts = {
      Vec2(0.0, 145.0),  Vec2(28.0, 145.0), Vec2(12.0, 150.0),
      Vec2(30.0, 150.0), Vec2(30.0, 170.0), Vec2(15.0, 152.0),
      Vec2(5.0, 170.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(11);

  verts = {
      Vec2(15.0, 100.0), Vec2(25.0, 100.0), Vec2(25.0, 110.0),
      Vec2(30.0, 115.0), Vec2(40.0, 115.0), Vec2(40.0, 125.0),
      Vec2(25.0, 125.0), Vec2(25.0, 140.0), Vec2(15.0, 140.0),
      Vec2(15.0, 127.0), Vec2(13.0, 125.0), Vec2(0.0, 125.0),
      Vec2(0.0, 115.0),  Vec2(15.0, 115.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(24);

  verts = {
      Vec2(105.0, 105.0), Vec2(120.0, 105.0), Vec2(120.0, 90.0),
      Vec2(130.0, 90.0),  Vec2(130.0, 105.0), Vec2(170.0, 105.0),
      Vec2(170.0, 115.0), Vec2(155.0, 115.0), Vec2(155.0, 135.0),
      Vec2(145.0, 135.0), Vec2(145.0, 115.0), Vec2(130.0, 115.0),
      Vec2(130.0, 130.0), Vec2(120.0, 130.0), Vec2(120.0, 115.0),
      Vec2(105.0, 115.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(22);

  // Cross uneven
  verts = {
      Vec2(170.0, 115.0), Vec2(180.0, 115.0), Vec2(180.0, 135.0),
      Vec2(200.0, 135.0), Vec2(200.0, 155.0), Vec2(180.0, 155.0),
      Vec2(180.0, 175.0), Vec2(170.0, 175.0), Vec2(170.0, 155.0),
      Vec2(150.0, 155.0), Vec2(150.0, 135.0), Vec2(170.0, 135.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(15);

  // First try with a hole
  verts = {
      Vec2(30.0, 150.0), Vec2(52.0, 157.0), Vec2(90.0, 152.0),
      Vec2(85.0, 200.0), Vec2(70.0, 190.0), Vec2(60.0, 193.0),
      Vec2(45.0, 180.0), Vec2(35.0, 165.0),
  };
  hole = {
      Vec2(48.0, 174.0), Vec2(75.0, 179.0), Vec2(69.0, 170.0),
      Vec2(78.0, 163.0), Vec2(70.0, 160.0),
  };
  skeletons.push_back(new Skeleton(verts, {hole}));
  polys.push_back(verts);
  polys.push_back(hole);
  skeleton_sizes.push_back(26);

  // Square with square hole
  verts = {
      Vec2(90.0, 170.0),
      Vec2(120.0, 170.0),
      Vec2(120.0, 200.0),
      Vec2(90.0, 200.0),
  };
  hole = {
      Vec2(100.0, 180.0),
      Vec2(100.0, 190.0),
      Vec2(110.0, 190.0),
      Vec2(110.0, 180.0),
  };
  skeletons.push_back(new Skeleton(verts, {hole}));
  polys.push_back(verts);
  polys.push_back(hole);
  skeleton_sizes.push_back(12);

  // Square with rotated square hole
  verts = {
      Vec2(90.0, 138.0),
      Vec2(120.0, 138.0),
      Vec2(120.0, 168.0),
      Vec2(90.0, 168.0),
  };
  hole = {
      Vec2(97.0, 153.0),
      Vec2(105.0, 161.0),
      Vec2(113.0, 153.0),
      Vec2(105.0, 145.0),
  };
  skeletons.push_back(new Skeleton(verts, {hole}));
  polys.push_back(verts);
  polys.push_back(hole);
  skeleton_sizes.push_back(16);

  // Buggy example (not visible on screen unless positions are tarnsformed).
  Vec2 pos(0.0, 0.0);  // Too see, change to, e.g., (200.0, 200.0).
  double scale = 1.0;  // Too see, change to, e.g., 4.0.
  verts = {
      Vec2(1.8485966558823452e-63, -2.2204460492503131e-15) * scale + pos,
      Vec2(1.7763568394002505e-15, -6.0000000000000009) * scale + pos,
      Vec2(7.0000000000000018, -6.0000000000000009) * scale + pos,
      Vec2(7.0000000000000018, -10.0) * scale + pos,
      Vec2(10.000000000000002, -10.0) * scale + pos,
      Vec2(10.0, -4.0788932203406723e-32) * scale + pos,
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(9);

  verts = {
      Vec2(0.0, 0.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(5.0, 0.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(7.5, 2.5) * 10.0 + Vec2(200.0, 10.0),
      Vec2(10.0, 0.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(10.0, 5.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(8.0, 5.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(10.0, 8.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(10.0, 10.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(5.0, 5.0) * 10.0 + Vec2(200.0, 10.0),
      Vec2(2.0, 10.0) * 10.0 + Vec2(200.0, 10.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(17);

  verts = {
      Vec2(0.0, 50.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(0.0, 0.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(30.0, 0.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(60.0, 0.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(90.0, 0.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(90.0, 120.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(45.0, 160.0) * 0.25 + Vec2(240.0, 90.0),
      Vec2(0.0, 120.0) * 0.25 + Vec2(240.0, 90.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(6);

  verts = {
      Vec2(0.0, 1.0) * 10.0 + Vec2(265.0, 90.0),
      Vec2(0.0, 0.0) * 10.0 + Vec2(265.0, 90.0),
      Vec2(1.0, 0.0) * 10.0 + Vec2(265.0, 90.0),
      Vec2(1.0, 1.0) * 10.0 + Vec2(265.0, 90.0),
  };
  skeletons.push_back(new Skeleton(verts, {}));
  polys.push_back(verts);
  skeleton_sizes.push_back(4);
}

#ifdef SKELETON_TEST_STANDALONE

void Draw() {
  glColor3f(1.0f, 0.0f, 0.0f);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();

  const float height_max = 200.0f;
  const float height_scale = 1.0f / (height_max * 0.5f);
  const float width_scale_factor = 1.6f;
  const float width_max = height_max * width_scale_factor;
  const float width_scale = height_scale / width_scale_factor;
  glScalef(width_scale, height_scale, 1.0f);
  glTranslatef(-width_max * 0.5f, -height_max * 0.5f, 0.0f);

  glBegin(GL_LINES);
  for (const auto& poly : polys) {
    size_t count = poly.size();
    glColor3f(1.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < count; ++i) {
      const Vec2& v1 = poly[i];
      const Vec2& v2 = poly[(i + 1) % count];
      glVertex2f(static_cast<float>(v1.x()), static_cast<float>(v1.y()));
      glVertex2f(static_cast<float>(v2.x()), static_cast<float>(v2.y()));
    }
  }
  glEnd();

  glColor3f(0.0f, 1.0f, 0.0f);
  glBegin(GL_LINES);
  for (const Skeleton* skel : skeletons) {
    for (size_t i = 0; i < skel->skeleton().size(); ++i) {
      const Vec2& v1 = skel->positions()[skel->skeleton()[i].v1];
      const Vec2& v2 = skel->positions()[skel->skeleton()[i].v2];
      glVertex2f(static_cast<float>(v1.x()), static_cast<float>(v1.y()));
      glVertex2f(static_cast<float>(v2.x()), static_cast<float>(v2.y()));
    }
  }
  glEnd();

  glPopMatrix();
}

void MakeOBJ() {
  IdxVecVec faces;
  Vec3Vec verts3;

  std::ofstream outfile("roofs.obj");
  assert(outfile.good());
  unsigned idx_offset = 0;

  for (const Skeleton* skel : skeletons) {
    Roof(*skel, 45.0, false, true, &verts3, &faces);

    for (const Vec3& v : verts3) {
      outfile << "v " << v.x() << " " << v.y() << " " << v.z() << std::endl;
    }
    for (const IdxVec& f : faces) {
      outfile << "f";
      for (unsigned i : f) {
        outfile << " " << (i + 1 + idx_offset);
      }
      outfile << std::endl;
    }

    idx_offset += static_cast<int>(verts3.size());
  }
  outfile.close();
}

static void ErrorCallback(int, const char* description) {
  fprintf(stderr, "Error: %s\n", description);
}

static void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

int main(int, char*[]) {
  GLFWwindow* window;

  glfwSetErrorCallback(ErrorCallback);
  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_RESIZABLE, false);
  window = glfwCreateWindow(1680, 1050, "Straight Skeleton Test Standalone",
                            NULL, NULL);
  if (!window) {
    glfwTerminate();
    return 1;
  }

  glfwSetKeyCallback(window, KeyCallback);
  glfwMakeContextCurrent(window);

  CreateSkeletons();

#ifndef NDEBUG
  for (size_t i = 0; i < skeletons.size(); ++i) {
    assert(skeletons[i]->skeleton().size() == skeleton_sizes[i]);
  }
#endif

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    Draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();

  MakeOBJ();

  for (Skeleton* skel : skeletons) {
    delete skel;
  }

  return 0;
}

#else

TEST(SkeletonTest, NumberOfArcsSamples) {
  CreateSkeletons();

  for (size_t i = 0; i < skeletons.size(); ++i) {
    EXPECT_EQ(skeletons[i]->skeleton().size(), skeleton_sizes[i])
        << "Number of arcs in skeleton " << i << " is wrong.";
  }
}

#endif
