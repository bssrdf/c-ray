//
//  vector.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 28/02/15.
//  Copyright (c) 2015 Valtteri Koskivuori. All rights reserved.
//

#include "vector.h"

//Main vertex arrays
vector *vertexArray;
int vertexCount;
vector *normalArray;
int normalCount;
vector *textureArray;
int textureCount;

/* Vector Functions */

//Create and return a vector with position values. Useful for hard-coded arrays.
vector vectorWithPos(double x, double y, double z) {
	vector result = {x, y, z};
	return result;
}

//Add two vectors and return the resulting vector
vector addVectors(vector *v1, vector *v2) {
	vector result = {v1->x + v2->x, v1->y + v2->y, v1->z + v2->z};
	return result;
}

//Compute length of a vector
float vectorLength(vector *v) {
    return sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
}

//Subtract two vectors and return the resulting vector
vector subtractVectors(vector *v1, vector *v2) {
	vector result = {v1->x - v2->x, v1->y - v2->y, v1->z - v2->z};
	return result;
}

//Multiply two vectors and return the dot product
float scalarProduct(vector *v1, vector *v2) {
	return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

//Multiply a vector by a scalar and return the resulting vector
vector vectorScale(double c, vector *v) {
	vector result = {v->x * c, v->y * c, v->z * c};
	return result;
}

//Calculate cross product and return the resulting vector
vector vectorCross(vector *v1, vector *v2) {
	vector result;
	
	result.x = (v1->y * v2->z) - (v1->z * v2->y);
	result.y = (v1->z * v2->x) - (v1->x * v2->z);
	result.z = (v1->x * v2->y) - (v1->y * v2->x);
	
	return result;
}

vector minVector(vector *v1, vector *v2) {
	vector result;
	
	result.x = min(v1->x, v2->x);
	result.y = min(v1->y, v2->y);
	result.z = min(v1->z, v2->z);
	
	return result;
}

vector maxVector(vector *v1, vector *v2) {
	vector result;
	
	result.x = max(v1->x, v2->x);
	result.y = max(v1->y, v2->y);
	result.z = max(v1->z, v2->z);
	
	return result;
}
