#pragma once

#include "Typedefs.h"
#include "Vector.h"

typedef struct Matrix4_t {
	f32 Data[4][4];
} Matrix4;

Matrix4 Matrix4_Identity();
Matrix4 Matrix4_Size(Vector3 v);
