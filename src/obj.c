//
//  obj.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 27/04/2017.
//  Copyright © 2017 Valtteri Koskivuori. All rights reserved.
//

#include "includes.h"
#include "obj.h"

#include "bbox.h"
#include "kdtree.h"


void addTransform(struct crayOBJ *obj, struct matrixTransform transform) {
	if (obj->transformCount == 0) {
		obj->transforms = (struct matrixTransform*)calloc(1, sizeof(struct matrixTransform));
	} else {
		obj->transforms = (struct matrixTransform*)realloc(obj->transforms, (obj->transformCount + 1) * sizeof(struct matrixTransform));
	}
	obj->transforms[obj->transformCount] = transform;
	obj->transformCount++;
}

void transformMesh(struct crayOBJ *object) {
	for (int tf = 0; tf < object->transformCount; tf++) {
		//Perform transforms
		
		//Evaluate amount of vectors to transform
		int vecCount = 0;
		for (int p = object->firstPolyIndex; p < (object->firstPolyIndex + object->polyCount); p++) {
			for (int v = 0; v < polygonArray[p].vertexCount; v++) {
				vecCount++;
			}
		}
		
		bool **alreadyTransformed = (bool**)calloc(vecCount, sizeof(bool*));
		for (int i = 0; i < vecCount; i++) {
			alreadyTransformed[i] = (bool*)calloc(3, sizeof(bool));
		}
		
		for (int p = object->firstPolyIndex; p < (object->firstPolyIndex + object->polyCount); p++) {
			for (int v = 0; v < polygonArray[p].vertexCount; v++) {
				if (!alreadyTransformed[p - object->firstPolyIndex][v]) {
					transformVector(&vertexArray[polygonArray[p].vertexIndex[v]], &object->transforms[tf]);
					alreadyTransformed[p - object->firstPolyIndex][v] = true;
				}
			}
		}
		for (int i = 0; i < vecCount; i++) {
			free(alreadyTransformed[i]);
		}
		free(alreadyTransformed);
	}
}

void freeOBJ(struct crayOBJ *obj) {
	if (obj->objName) {
		free(obj->objName);
	}
	for (int i = 0; i < obj->materialCount; i++) {
		freeMaterial(&obj->materials[i]);
	}
	free(obj->materials);
	
	if (obj->tree) {
		freeTree(obj->tree);
	}
	free(obj->tree);
	
	free(obj->transforms);
}
