//
//  converter.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 18/08/2017.
//  Copyright © 2017 Valtteri Koskivuori. All rights reserved.
//

#include "includes.h"
#include "converter.h"

#include "obj_parser.h"
#include "poly.h"

/**
 Convert a given OBJ loader vector into a c-ray vector
 
 @param vec OBJ loader vector
 @return c-ray vector
 */
struct vector vectorFromObj(obj_vector *vec) {
	struct vector vector;
	vector.x = vec->e[0];
	vector.y = vec->e[1];
	vector.z = vec->e[2];
	return vector;
}


struct coord coordFromObj(obj_vector *vec) {
	struct coord coordinate;
	coordinate.x = vec->e[0];
	coordinate.y = vec->e[1];
	return coordinate;
}

/**
 Convert a given OBJ loader polygon into a c-ray polygon
 
 @param face OBJ loader polygon
 @param firstVertexIndex First vertex index of the new polygon
 @param firstNormalIndex First normal index of the new polygon
 @param firstTextureIndex First texture index of the new polygon
 @param polyIndex polygonArray index offset
 @return c-ray polygon
 */
struct poly polyFromObj(obj_face *face, int firstVertexIndex, int firstNormalIndex, int firstTextureIndex, int polyIndex) {
	struct poly polygon;
	if (face->normal_index[0] == -1) {
		polygon.hasNormals = false;
	} else {
		polygon.hasNormals = true;
		polygon.vertexCount = face->vertex_count;
		polygon.materialIndex = face->material_index;
		polygon.polyIndex = polyIndex;
		for (int i = 0; i < polygon.vertexCount; i++) {
			polygon.vertexIndex[i] = firstVertexIndex + face->vertex_index[i];
		}
		for (int i = 0; i < polygon.vertexCount; i++) {
			polygon.normalIndex[i] = firstNormalIndex + face->normal_index[i];
		}
		for (int i = 0; i < polygon.vertexCount; i++) {
			polygon.textureIndex[i] = firstTextureIndex + face->texture_index[i];
		}
		
	}
	return polygon;
}


/**
 Convert a given OBJ loader material into a c-ray material
 
 @param mat OBJ loader material
 @return c-ray material
 */
struct material materialFromObj(obj_material *mat) {
	struct material newMat;
	
	newMat.name = (char*)calloc(256, sizeof(char*));
	newMat.textureFilePath = (char*)calloc(500, sizeof(char*));
	
	newMat.hasTexture = false;
	
	for (int i = 0; i < 255; i++) {
		newMat.name[i] = mat->name[i];
		newMat.name[255] = '\0';
	}
	
	for (int i = 0; i < 500; i++) {
		newMat.textureFilePath[i] = mat->texture_filename[i];
		newMat.textureFilePath[499] = '\0';
	}
	
	newMat.diffuse.red   = mat->diff[0];
	newMat.diffuse.green = mat->diff[1];
	newMat.diffuse.blue  = mat->diff[2];
	newMat.diffuse.alpha = 0;
	newMat.ambient.red   = mat->amb[0];
	newMat.ambient.green = mat->amb[1];
	newMat.ambient.blue  = mat->amb[2];
	newMat.ambient.alpha = 0;
	newMat.specular.red  = mat->spec[0];
	newMat.specular.green= mat->spec[1];
	newMat.specular.blue = mat->spec[2];
	newMat.specular.alpha= 0;
	newMat.emission.red  = mat->emit[0];
	newMat.emission.green= mat->emit[1];
	newMat.emission.blue = mat->emit[2];
	newMat.emission.alpha = 0;
	newMat.reflectivity  = mat->reflect;
	newMat.refractivity  = mat->refract;
	newMat.IOR           = mat->refract_index;
	newMat.glossiness    = mat->glossy;
	newMat.transparency  = mat->trans;
	newMat.sharpness     = 0;
	return newMat;
}
