#include "Matrix.h"

Matrix4 Matrix4_Identity() {
	Matrix4 result = (Matrix4){
		.Data = {
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		},
	};
	return result;
}

Matrix4 Matrix4_Size(Vector3 v) {
	Matrix4 result = (Matrix4){
		.Data = {
			{ v.x,  0.0f, 0.0f, 0.0f },
			{ 0.0f, v.y,  0.0f, 0.0f },
			{ 0.0f, 0.0f, v.z,  0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		},
	};
	return result;
}
