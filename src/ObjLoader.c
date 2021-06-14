#include "ObjLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_FIRST_OF_MOVE(str, cmpStr) (strncmp(str, cmpStr, sizeof(cmpStr) - 1) == 0 ? str += (sizeof(cmpStr) - 1), true : false)

static b8 ObjMesh_LoadMaterial(ObjMesh* mesh, char* source) {
	char* chr = source;
	while (*chr != '\0') {
		if (*chr == '#') {
			while (*chr != '\n' && *chr != '\0') {
				chr++;
			}
		} else if (MATCH_FIRST_OF_MOVE(chr, "newmtl ")) {
			char* start = chr;
			u64 length = 0;
			while (*chr != '\n' && *chr != '\0') {
				length++;
				chr++;
			}

			char* name = malloc((length + 1) * sizeof(name[0]));
			if (!name) {
				return false;
			}

			memcpy(name, start, length * sizeof(name[0]));
			name[length] = '\0';

			mesh->MaterialCount++;
			mesh->Materials = realloc(mesh->Materials, mesh->MaterialCount * sizeof(mesh->Materials[0]));
			if (!mesh->Materials) {
				free(name);
				return false;
			}

			memset(&mesh->Materials[mesh->MaterialCount - 1], 0, sizeof(mesh->Materials[0]));

			mesh->Materials[mesh->MaterialCount - 1].Name = name;
		} else if (MATCH_FIRST_OF_MOVE(chr, "Ka ")) {
			mesh->Materials[mesh->MaterialCount - 1].Ka.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ka.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ka.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Kd ")) {
			mesh->Materials[mesh->MaterialCount - 1].Kd.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Kd.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Kd.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Ks ")) {
			mesh->Materials[mesh->MaterialCount - 1].Ks.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ks.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ks.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Ke ")) {
			mesh->Materials[mesh->MaterialCount - 1].Ke.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ke.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Ke.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Kt ")) {
			mesh->Materials[mesh->MaterialCount - 1].Kt.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Kt.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Kt.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Ns ")) {
			mesh->Materials[mesh->MaterialCount - 1].Ns = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Ni ")) {
			mesh->Materials[mesh->MaterialCount - 1].Ni = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "Tf ")) {
			mesh->Materials[mesh->MaterialCount - 1].Tf.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Tf.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].Tf.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "d ")) {
			mesh->Materials[mesh->MaterialCount - 1].d.x = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].d.y = strtof(chr, &chr);
			mesh->Materials[mesh->MaterialCount - 1].d.z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "illum ")) {
			mesh->Materials[mesh->MaterialCount - 1].illum = strtoul(chr, &chr, 10);
		} else if (*chr == '\n') {
			chr++;
			continue;
		} else {
			ASSERT(false);
		}

		if (*chr == '\0') {
			break;
		} else if (*chr == '\n') {
			chr++;
		} else {
			ASSERT(false);
		}
	}

	return true;
}

static b8 ObjMesh_LoadMeshes(ObjMesh* mesh, char* source) {
	u64 currentMaterialIndex = ~0ull;
	u64 currentObjectIndex = ~0ull;
	u64 currentFaceIndex = ~0ull;

	char* chr = source;
	while (*chr != '\0') {
		if (*chr == '#') {
			while (*chr != '\n' && *chr != '\0') {
				chr++;
			}
		} else if (MATCH_FIRST_OF_MOVE(chr, "mtllib ")) {
			char* start = chr;
			u64 length = 0;
			while (*chr != '\n' && *chr != '\0') {
				length++;
				chr++;
			}

			char* path = malloc((length + 1) * sizeof(path[0]));
			if (!path) {
				return false;
			}

			memcpy(path, start, length * sizeof(path[0]));
			path[length] = '\0';

			FILE* file = fopen(path, "r");
			if (!file) {
				free(path);
				return false;
			}
			
			free(path);

			fseek(file, 0, SEEK_END);
			u64 materialSourceLength = ftell(file);
			fseek(file, 0, SEEK_SET);

			if (materialSourceLength == 0) {
				fclose(file);
				return false;
			}

			char* materialSource = malloc((materialSourceLength + 1) * sizeof(materialSource[0]));
			if (!materialSource) {
				fclose(file);
				return false;
			}

			fread(materialSource, sizeof(char), materialSourceLength, file);
			materialSource[materialSourceLength] = '\0';

			fclose(file);

			if (!ObjMesh_LoadMaterial(mesh, materialSource)) {
				free(materialSource);
				return false;
			}

			free(materialSource);
		} else if (MATCH_FIRST_OF_MOVE(chr, "usemtl ")) {
			char* start = chr;
			u64 length = 0;
			while (*chr != '\n' && *chr != '\0') {
				length++;
				chr++;
			}

			char* name = malloc((length + 1) * sizeof(name[0]));
			if (!name) {
				return false;
			}

			memcpy(name, start, length * sizeof(name[0]));
			name[length] = '\0';

			u64 index = ~0ull;
			for (u64 i = 0; i < mesh->MaterialCount; i++) {
				if (strcmp(mesh->Materials[i].Name, name) == 0) {
					index = i;
					break;
				}
			}
			
			free(name);

			if (index == ~0ull) {
				return false;
			}

			currentMaterialIndex = index;
		} else if (MATCH_FIRST_OF_MOVE(chr, "v ")) {
			mesh->PositionCount++;
			mesh->Positions = realloc(mesh->Positions, mesh->PositionCount * sizeof(mesh->Positions[0]));
			if (!mesh->Positions) {
				return false;
			}
			
			memset(&mesh->Positions[mesh->PositionCount - 1], 0, sizeof(mesh->Positions[0]));

			mesh->Positions[mesh->PositionCount - 1].x = strtof(chr, &chr);
			mesh->Positions[mesh->PositionCount - 1].y = strtof(chr, &chr);
			mesh->Positions[mesh->PositionCount - 1].z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "vt ")) {
			mesh->TexCoordCount++;
			mesh->TexCoords = realloc(mesh->TexCoords, mesh->TexCoordCount * sizeof(mesh->TexCoords[0]));
			if (!mesh->TexCoords) {
				return false;
			}
			
			memset(&mesh->TexCoords[mesh->TexCoordCount - 1], 0, sizeof(mesh->TexCoords[0]));

			mesh->TexCoords[mesh->TexCoordCount - 1].x = strtof(chr, &chr);
			mesh->TexCoords[mesh->TexCoordCount - 1].y = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "vn ")) {
			mesh->NormalCount++;
			mesh->Normals = realloc(mesh->Normals, mesh->NormalCount * sizeof(mesh->Normals[0]));
			if (!mesh->Normals) {
				return false;
			}
			
			memset(&mesh->Normals[mesh->NormalCount - 1], 0, sizeof(mesh->Normals[0]));

			mesh->Normals[mesh->NormalCount - 1].x = strtof(chr, &chr);
			mesh->Normals[mesh->NormalCount - 1].y = strtof(chr, &chr);
			mesh->Normals[mesh->NormalCount - 1].z = strtof(chr, &chr);
		} else if (MATCH_FIRST_OF_MOVE(chr, "s ")) {
			while (*chr != '\n' && *chr != '\0') {
				chr++;
			}
		} else if (MATCH_FIRST_OF_MOVE(chr, "f ")) {
			if (currentObjectIndex == ~0ull) return false;
			if (currentMaterialIndex == ~0ull) return false;

			mesh->Objects[currentObjectIndex].FaceCount++;

			mesh->FaceCount++;
			mesh->Faces = realloc(mesh->Faces, mesh->FaceCount * sizeof(mesh->Faces[0]));
			if (!mesh->Faces) {
				return false;
			}

			mesh->Faces[mesh->FaceCount - 1].MaterialIndex = currentMaterialIndex;

			mesh->Faces[mesh->FaceCount - 1].PositionIndices[0] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].TexCoordIndices[0] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].NormalIndices[0] = strtoull(chr, &chr, 10) - 1;
			
			ASSERT(*chr++ == ' ');
			
			mesh->Faces[mesh->FaceCount - 1].PositionIndices[1] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].TexCoordIndices[1] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].NormalIndices[1] = strtoull(chr, &chr, 10) - 1;
			
			ASSERT(*chr++ == ' ');
			
			mesh->Faces[mesh->FaceCount - 1].PositionIndices[2] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].TexCoordIndices[2] = strtoull(chr, &chr, 10) - 1;
			ASSERT(*chr++ == '/');
			mesh->Faces[mesh->FaceCount - 1].NormalIndices[2] = strtoull(chr, &chr, 10) - 1;
		} else if (MATCH_FIRST_OF_MOVE(chr, "o ")) {
			char* start = chr;
			u64 length = 0;
			while (*chr != '\n' && *chr != '\0') {
				length++;
				chr++;
			}

			char* name = malloc((length + 1) * sizeof(name[0]));
			if (!name) {
				return false;
			}

			memcpy(name, start, length * sizeof(name[0]));
			name[length] = '\0';

			currentObjectIndex++;
			mesh->ObjectCount++;
			mesh->Objects = realloc(mesh->Objects, mesh->ObjectCount * sizeof(mesh->Objects[0]));
			if (!mesh->Objects) {
				free(name);
				return false;
			}

			mesh->Objects[currentObjectIndex].Name = name;
			mesh->Objects[currentObjectIndex].FaceOffset = currentFaceIndex + 1;
			mesh->Objects[currentObjectIndex].FaceCount = 0;
		} else if (*chr == '\n') {
			chr++;
			continue;
		} else {
			ASSERT(false);
		}

		if (*chr == '\0') {
			break;
		} else if (*chr == '\n') {
			chr++;
		} else {
			ASSERT(false);
		}
	}

	return true;
}

b8 ObjMesh_Create(ObjMesh* mesh, const char* filepath) {
	*mesh = (ObjMesh){};

	FILE* file = fopen(filepath, "r");
	if (!file) {
		return false;
	}

	fseek(file, 0, SEEK_END);
	u64 sourceLength = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (sourceLength == 0) {
		fclose(file);
		return false;
	}

	char* source = malloc((sourceLength + 1) * sizeof(source[0]));
	if (!source) {
		fclose(file);
		return false;
	}

	fread(source, sizeof(char), sourceLength, file);
	source[sourceLength] = '\0';

	fclose(file);

	if (!ObjMesh_LoadMeshes(mesh, source)) {
		free(source);
		return false;
	}

	free(source);
	return true;
}

void ObjMesh_Destory(ObjMesh* mesh) {
	ASSERT(false);
}
