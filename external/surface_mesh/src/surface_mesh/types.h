//=============================================================================
// Copyright (C) 2013 Graphics & Geometry Group, Bielefeld University
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public License
// as published by the Free Software Foundation, version 2.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================


#ifndef SURFACE_MESH_TYPES_H
#define SURFACE_MESH_TYPES_H

#define SURFACE_MESH_VERSION 1.1

//== INCLUDES =================================================================


//#include <surface_mesh/Vector.h>
#include "shapeml/geometry/vector_types.h"


//=============================================================================


namespace surface_mesh {


//=============================================================================


/// Scalar type
typedef double Scalar;

/// Point type
//typedef Vector<Scalar,3> Point;
typedef shapeml::Vec3 Point;

/// 3D vector type
//typedef Vector<Scalar,3> Vec3;
typedef shapeml::Vec3f Vec3f;
typedef shapeml::Vec2f Vec2f;

/// Normal type
//typedef Vector<Scalar,3> Normal;
typedef shapeml::Vec3 Normal;

/// Color type
//typedef Vector<Scalar,3> Color;
typedef shapeml::Vec3 Color;

/// Texture coordinate type
//typedef Vector<Scalar,3> Texture_coordinate;
typedef shapeml::Vec3 Texture_coordinate;


//=============================================================================
} // namespace surface_mesh
//=============================================================================
#endif // SURFACE_MESH_TYPES_H
//============================================================================
