#pragma once

#include "Typedefs.h"
#include "Vector.h"

typedef struct ObjMaterial_t {
	char* Name;

	Vector3 Ka; // Ambient
	Vector3 Kd; // Diffuse
	Vector3 Ks; // Specular
	Vector3 Ke; // Emission
	Vector3 Kt; // Transmittance
	f32 Ns;     // Shininess
	f32 Ni;     // Index of Refraction
	Vector3 Tf; // Transmission Filter
	Vector3 d;  // Alpha
	s32 illum;  // Illumination Model
} ObjMaterial;

typedef struct ObjFace_t {
	u64 PositionIndices[3];
	u64 NormalIndices[3];
	u64 TexCoordIndices[3];
	u32 MaterialIndex;
} ObjFace;

typedef struct ObjObject_t {
	char* Name;

	u64 FaceOffset;
	u64 FaceCount;
} ObjObject;

typedef struct ObjMesh_t {
	Vector3* Positions;
	u64 PositionCount;

	Vector3* Normals;
	u64 NormalCount;

	Vector2* TexCoords;
	u64 TexCoordCount;

	ObjMaterial* Materials;
	u64 MaterialCount;

	ObjFace* Faces;
	u64 FaceCount;

	ObjObject* Objects;
	u64 ObjectCount;
} ObjMesh;

b8 ObjMesh_Create(ObjMesh* mesh, const char* filepath);
void ObjMesh_Destory(ObjMesh* mesh);
