//
//  scene.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 28/02/15.
//  Copyright (c) 2015-2018 Valtteri Koskivuori. All rights reserved.
//

#include "includes.h"
#include "scene.h"

#include "camera.h"
#include "light.h"
#include "obj_parser.h"
#include "kdtree.h"
#include "filehandler.h"
#include "converter.h"
#include "renderer.h"
#include "cJSON.h"
#include "ui.h"
#include "logging.h"
#include "tile.h"
#include "timer.h"

#define TOKEN_DEBUG_ENABLED false

void copyString(const char *source, char **destination);
size_t getDelim(char **lineptr, size_t *n, int delimiter, FILE *stream);

/**
 Extract the filename from a given file path

 @param input File path to be processed
 @return Filename string, including file type extension
 */
char *getFileName(char *input) {
	char *fn;
	
	/* handle trailing '/' e.g.
	 input == "/home/me/myprogram/" */
	if (input[(strlen(input) - 1)] == '/')
		input[(strlen(input) - 1)] = '\0';
	
	(fn = strrchr(input, '/')) ? ++fn : (fn = input);
	
	return fn;
}

/**
 Get amount of logical processing cores on the system
 
 @return Amount of logical processing cores
 */
int getSysCores() {
#ifdef __APPLE__
	int nm[2];
	size_t len = 4;
	uint32_t count;
	
	nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
	sysctl(nm, 2, &count, &len, NULL, 0);
	
	if (count < 1) {
		nm[1] = HW_NCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);
		if (count < 1) {
			count = 1;
		}
	}
	return count;
#elif _WIN32
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwNumberOfProcessors;
#elif __linux__
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

void addMaterialOBJ(struct crayOBJ *obj, struct material newMaterial);

struct crayOBJ *lastObj(struct renderer *r) {
	return &r->scene->objs[r->scene->objCount - 1];
}

struct sphere *lastSphere(struct renderer *r) {
	return &r->scene->spheres[r->scene->sphereCount - 1];
}

struct texture *newTexture(char *filePath) {
	struct texture *newTexture = (struct texture*)calloc(1, sizeof(struct texture));
	newTexture->imgData = NULL;
	newTexture->width = (unsigned int*)calloc(1, sizeof(unsigned int));
	newTexture->height = (unsigned int*)calloc(1, sizeof(unsigned int));
	
	int err = lodepng_decode24_file(&newTexture->imgData, newTexture->width, newTexture->height, filePath);
	if (err != 0) {
		logr(warning, "Texture loading error: %s\n", lodepng_error_text(err));
	}
	return newTexture;
}

void loadOBJTextures(struct crayOBJ *obj) {
	for (int i = 0; i < obj->materialCount; i++) {
		if (strcmp(obj->materials[i].textureFilePath, "")) {
			obj->materials[i].texture = newTexture(obj->materials[i].textureFilePath);
			obj->materials[i].hasTexture = true;
		}
	}
}

bool loadOBJ(struct renderer *r, char *inputFileName) {
	logr(info, "Loading OBJ %s%s\n", r->inputFilePath, inputFileName);
	
	obj_scene_data data;
	if (parse_obj_scene(&data, inputFileName, r->inputFilePath) == 0) {
		logr(warning, "OBJ %s not found!\n", getFileName(inputFileName));
		return false;
	}
	
	//Create crayOBJ to keep track of objs
	r->scene->objs = (struct crayOBJ*)realloc(r->scene->objs, (r->scene->objCount + 1) * sizeof(struct crayOBJ));
	//Vertex data
	r->scene->objs[r->scene->objCount].firstVectorIndex = vertexCount;
	r->scene->objs[r->scene->objCount].vertexCount = data.vertex_count;
	//Normal data
	r->scene->objs[r->scene->objCount].firstNormalIndex = normalCount;
	r->scene->objs[r->scene->objCount].normalCount = data.vertex_normal_count;
	//Texture vector data
	r->scene->objs[r->scene->objCount].firstTextureIndex = textureCount;
	r->scene->objs[r->scene->objCount].textureCount = data.vertex_texture_count;
	//Poly data
	r->scene->objs[r->scene->objCount].firstPolyIndex = polyCount;
	r->scene->objs[r->scene->objCount].polyCount = data.face_count;
	//Transforms init
	r->scene->objs[r->scene->objCount].transformCount = 0;
	r->scene->objs[r->scene->objCount].transforms = (struct matrixTransform*)malloc(sizeof(struct matrixTransform));
	
	r->scene->objs[r->scene->objCount].materialCount = 0;
	//Set name
	copyString(getFileName(inputFileName), &r->scene->objs[r->scene->objCount].objName);
	
	//Update vector and poly counts
	vertexCount += data.vertex_count;
	normalCount += data.vertex_normal_count;
	textureCount += data.vertex_texture_count;
	polyCount += data.face_count;
	
	//Data loaded, now convert everything
	//Convert vectors
	vertexArray = (struct vector*)realloc(vertexArray, vertexCount * sizeof(struct vector));
	for (int i = 0; i < data.vertex_count; i++) {
		vertexArray[r->scene->objs[r->scene->objCount].firstVectorIndex + i] = vectorFromObj(data.vertex_list[i]);
	}
	
	//Convert normals
	normalArray = (struct vector*)realloc(normalArray, normalCount * sizeof(struct vector));
	for (int i = 0; i < data.vertex_normal_count; i++) {
		normalArray[r->scene->objs[r->scene->objCount].firstNormalIndex + i] = vectorFromObj(data.vertex_normal_list[i]);
	}
	//Convert texture vectors
	textureArray = (struct coord*)realloc(textureArray, textureCount * sizeof(struct coord));
	for (int i = 0; i < data.vertex_texture_count; i++) {
		textureArray[r->scene->objs[r->scene->objCount].firstTextureIndex + i] = coordFromObj(data.vertex_texture_list[i]);
	}
	//Convert polygons
	polygonArray = (struct poly*)realloc(polygonArray, polyCount * sizeof(struct poly));
	for (int i = 0; i < data.face_count; i++) {
		polygonArray[r->scene->objs[r->scene->objCount].firstPolyIndex + i] = polyFromObj(data.face_list[i],
																							r->scene->objs[r->scene->objCount].firstVectorIndex,
																							r->scene->objs[r->scene->objCount].firstNormalIndex,
																							r->scene->objs[r->scene->objCount].firstTextureIndex,
																							r->scene->objs[r->scene->objCount].firstPolyIndex + i);
	}
	
	r->scene->objs[r->scene->objCount].materials = (struct material*)calloc(1, sizeof(struct material));
	//Parse materials
	if (data.material_count == 0) {
		//No material, set to something obscene to make it noticeable
		r->scene->objs[r->scene->objCount].materials = (struct material*)calloc(1, sizeof(struct material));
		*r->scene->objs[r->scene->objCount].materials = newMaterial(colorWithValues(255.0/255.0, 20.0/255.0, 147.0/255.0, 0), 0);
	} else {
		//Loop to add materials to obj (We already set the material indices in polyFromObj)
		for (int i = 0; i < data.material_count; i++) {
			addMaterialOBJ(&r->scene->objs[r->scene->objCount], materialFromObj(data.material_list[i]));
		}
	}
	
	//Load textures for OBJs
	loadOBJTextures(&r->scene->objs[r->scene->objCount]);
	
	//Delete OBJ data
	delete_obj_data(&data);
	
	//Obj added, update count
	r->scene->objCount++;
	return true;
}

//FIXME: change + 1 to ++scene->someCount and just pass the count to array access
//In the future, maybe just pass a list and size and copy at once to save time (large counts)
void addSphere(struct world *scene, struct sphere newSphere) {
	scene->spheres = (struct sphere*)realloc(scene->spheres, (scene->sphereCount + 1) * sizeof(struct sphere));
	scene->spheres[scene->sphereCount++] = newSphere;
}

void addMaterial(struct world *scene, struct material newMaterial) {
	scene->materials = (struct material*)realloc(scene->materials, (scene->materialCount + 1) * sizeof(struct material));
	scene->materials[scene->materialCount++] = newMaterial;
}

void addMaterialOBJ(struct crayOBJ *obj, struct material newMaterial) {
	obj->materials = (struct material*)realloc(obj->materials, (obj->materialCount + 1) * sizeof(struct material));
	obj->materials[obj->materialCount++] = newMaterial;
}

void addLight(struct world *scene, struct light newLight) {
	scene->lights = (struct light*)realloc(scene->lights, (scene->lightCount + 1) * sizeof(struct light));
	scene->lights[scene->lightCount++] = newLight;
}

void transformMeshes(struct world *scene) {
	logr(info, "Running transforms...\n");
	for (int i = 0; i < scene->objCount; ++i) {
		transformMesh(&scene->objs[i]);
	}
}

void computeKDTrees(struct world *scene) {
	logr(info, "Computing KD-trees...\n");
	for (int i = 0; i < scene->objCount; ++i) {
		int *polys = (int*)calloc(scene->objs[i].polyCount, sizeof(int));
		for (int j = 0; j < scene->objs[i].polyCount; j++) {
			polys[j] = scene->objs[i].firstPolyIndex + j;
		}
		scene->objs[i].tree = buildTree(polys, scene->objs[i].polyCount, 0);
	}
}

void addCamTransform(struct camera *cam, struct matrixTransform transform) {
	if (cam->transformCount == 0) {
		cam->transforms = (struct matrixTransform*)calloc(1, sizeof(struct matrixTransform));
	} else {
		cam->transforms = (struct matrixTransform*)realloc(cam->transforms, (cam->transformCount + 1) * sizeof(struct matrixTransform));
	}
	
	cam->transforms[cam->transformCount] = transform;
	cam->transformCount++;
}

void addCamTransforms(struct camera *cam, struct matrixTransform *transforms, int count) {
	for (int i = 0; i < count; i++) {
		addCamTransform(cam, transforms[i]);
	}
}

void printSceneStats(struct world *scene) {
	logr(info, "Scene parsing complete\n");
	logr(info, "Totals: %iV, %iN, %iP, %iS, %iL\n",
		   vertexCount,
		   normalCount,
		   polyCount,
		   scene->sphereCount,
		   scene->lightCount);
}

char *loadFile(char *inputFileName) {
	FILE *f = fopen(inputFileName, "rb");
	if (!f) {
		logr(warning, "No file found at %s", inputFileName);
		return NULL;
	}
	char *buf = NULL;
	size_t len;
	size_t bytesRead = getDelim(&buf, &len, '\0', f);
	if (bytesRead != -1) {
		logr(info, "%zi bytes of input JSON loaded, parsing...\n", bytesRead);
	} else {
		logr(warning, "Failed to read input JSON from %s", inputFileName);
		return NULL;
	}
	return buf;
}

struct matrixTransform parseTransform(const cJSON *data) {
	
	cJSON *type = cJSON_GetObjectItem(data, "type");
	if (!cJSON_IsString(type)) {
		logr(warning, "Failed to parse transform! No type found\n");
		logr(warning, "Transform data: %s\n", cJSON_Print(data));
	}
	
	//FIXME: Use parseCoordinate() for this
	
	cJSON *degrees = NULL;
	cJSON *scale = NULL;
	cJSON *X = NULL;
	cJSON *Y = NULL;
	cJSON *Z = NULL;
	
	bool validDegrees = false;
	bool validScale = false;
	bool validCoords = false;
	
	degrees = cJSON_GetObjectItem(data, "degrees");
	scale = cJSON_GetObjectItem(data, "scale");
	X = cJSON_GetObjectItem(data, "X");
	Y = cJSON_GetObjectItem(data, "Y");
	Z = cJSON_GetObjectItem(data, "Z");
	
	if (degrees != NULL && cJSON_IsNumber(degrees)) {
		validDegrees = true;
	}
	if (scale != NULL && cJSON_IsNumber(scale)) {
		validScale = true;
	}
	if (X != NULL && Y != NULL && Z != NULL) {
		if (cJSON_IsNumber(X) && cJSON_IsNumber(Y) && cJSON_IsNumber(Z)) {
			validCoords = true;
		}
	}
	
	if (strcmp(type->valuestring, "rotateX") == 0) {
		if (validDegrees) {
			return newTransformRotateX(degrees->valuedouble);
		} else {
			logr(warning, "Found rotateX transform with no valid degrees value given.\n");
		}
	} else if (strcmp(type->valuestring, "rotateY") == 0) {
		if (validDegrees) {
			return newTransformRotateY(degrees->valuedouble);
		} else {
			logr(warning, "Found rotateY transform with no valid degrees value given.\n");
		}
	} else if (strcmp(type->valuestring, "rotateZ") == 0) {
		if (validDegrees) {
			return newTransformRotateZ(degrees->valuedouble);
		} else {
			logr(warning, "Found rotateZ transform with no valid degrees value given.\n");
		}
	} else if (strcmp(type->valuestring, "translate") == 0) {
		if (validCoords) {
			return newTransformTranslate(X->valuedouble, Y->valuedouble, Z->valuedouble);
		} else {
			logr(warning, "Found translate transform with no valid coords given.\n");
		}
	} else if (strcmp(type->valuestring, "scale") == 0) {
		if (validCoords) {
			return newTransformScale(X->valuedouble, Y->valuedouble, Z->valuedouble);
		} else {
			logr(warning, "Found scale transform with no valid scale value given.\n");
		}
	} else if (strcmp(type->valuestring, "scaleUniform") == 0) {
		if (validScale) {
			return newTransformScaleUniform(scale->valuedouble);
		} else {
			logr(warning, "Found scaleUniform transform with no valid scale value given.\n");
		}
	} else {
		logr(warning, "Found an invalid transform\n");
	}
	
	return emptyTransform();
}

//Parse JSON array of transforms, and return a pointer to an array of corresponding matrixTransforms
struct matrixTransform *parseTransforms(const cJSON *data) {
	
	int transformCount = cJSON_GetArraySize(data);
	struct matrixTransform *transforms = (struct matrixTransform*)calloc(transformCount, sizeof(struct matrixTransform));
	
	cJSON *transform = NULL;
	
	for (int i = 0; i < transformCount; i++) {
		transform = cJSON_GetArrayItem(data, i);
		transforms[i] = parseTransform(transform);
	}
	return transforms;
}

int parseRenderer(struct renderer *r, const cJSON *data) {
	const cJSON *threadCount = NULL;
	const cJSON *sampleCount = NULL;
	const cJSON *antialiasing = NULL;
	const cJSON *tileWidth = NULL;
	const cJSON *tileHeight = NULL;
	const cJSON *tileOrder = NULL;
	
	threadCount = cJSON_GetObjectItem(data, "threadCount");
	if (cJSON_IsNumber(threadCount)) {
		if (threadCount->valueint > 0) {
			r->threadCount = threadCount->valueint;
		} else {
			r->threadCount = getSysCores();
		}
	} else {
		logr(warning, "Invalid threadCount while parsing renderer\n");
		return -1;
	}
	
	sampleCount = cJSON_GetObjectItem(data, "sampleCount");
	if (cJSON_IsNumber(sampleCount)) {
		if (sampleCount->valueint >= 1) {
			r->sampleCount = sampleCount->valueint;
		} else {
			r->sampleCount = 1;
		}
	} else {
		logr(warning, "Invalid sampleCount while parsing renderer\n");
		return -1;
	}
	
	antialiasing = cJSON_GetObjectItem(data, "antialiasing");
	if (cJSON_IsBool(antialiasing)) {
		r->antialiasing = cJSON_IsTrue(antialiasing);
	} else {
		logr(warning, "Invalid antialiasing bool while parsing renderer\n");
		return -1;
	}
	
	tileWidth = cJSON_GetObjectItem(data, "tileWidth");
	if (cJSON_IsNumber(tileWidth)) {
		if (tileWidth->valueint >= 1) {
			r->tileWidth = tileWidth->valueint;
		} else {
			r->tileWidth = 1;
		}
	} else {
		logr(warning, "Invalid tileWidth while parsing renderer\n");
		return -1;
	}
	
	tileHeight = cJSON_GetObjectItem(data, "tileHeight");
	if (cJSON_IsNumber(tileHeight)) {
		if (tileHeight->valueint >= 1) {
			r->tileHeight = tileHeight->valueint;
		} else {
			r->tileHeight = 1;
		}
	} else {
		logr(warning, "Invalid tileHeight while parsing renderer\n");
		return -1;
	}
	
	tileOrder = cJSON_GetObjectItem(data, "tileOrder");
	if (cJSON_IsString(tileOrder)) {
		if (strcmp(tileOrder->valuestring, "normal") == 0) {
			r->tileOrder = renderOrderNormal;
		} else if (strcmp(tileOrder->valuestring, "random") == 0) {
			r->tileOrder = renderOrderRandom;
		} else if (strcmp(tileOrder->valuestring, "topToBottom") == 0) {
			r->tileOrder = renderOrderTopToBottom;
		} else if (strcmp(tileOrder->valuestring, "fromMiddle") == 0) {
			r->tileOrder = renderOrderFromMiddle;
		} else if (strcmp(tileOrder->valuestring, "toMiddle") == 0) {
			r->tileOrder = renderOrderToMiddle;
		} else {
			r->tileOrder = renderOrderNormal;
		}
	} else {
		logr(warning, "Invalid tileOrder while parsing renderer\n");
		return -1;
	}
	
	return 0;
}

int parseDisplay(struct renderer *r, const cJSON *data) {
	
#ifdef UI_ENABLED
	const cJSON *isFullscreen = NULL;
	const cJSON *isBorderless = NULL;
	const cJSON *windowScale = NULL;
	
	isFullscreen = cJSON_GetObjectItem(data, "isFullscreen");
	if (cJSON_IsBool(isFullscreen)) {
		r->mainDisplay->isFullScreen = cJSON_IsTrue(isFullscreen);
	}
	
	isBorderless = cJSON_GetObjectItem(data, "isBorderless");
	if (cJSON_IsBool(isBorderless)) {
		r->mainDisplay->isBorderless = cJSON_IsTrue(isBorderless);
	}
	windowScale = cJSON_GetObjectItem(data, "windowScale");
	if (cJSON_IsNumber(windowScale)) {
		if (windowScale->valuedouble >= 0) {
			r->mainDisplay->windowScale = windowScale->valuedouble;
		} else {
			r->mainDisplay->windowScale = 0.5;
		}
	}
#endif
	return 0;
}

int parseCamera(struct renderer *r, const cJSON *data) {
	
	const cJSON *FOV = NULL;
	const cJSON *aperture = NULL;
	const cJSON *transforms = NULL;
	
	FOV = cJSON_GetObjectItem(data, "FOV");
	if (cJSON_IsNumber(FOV)) {
		if (FOV->valuedouble >= 0.0) {
			r->scene->camera->FOV = FOV->valuedouble;
		} else {
			r->scene->camera->FOV = 80.0;
		}
	} else {
		logr(warning, "No FOV for camera found");
		return -1;
	}
	
	aperture = cJSON_GetObjectItem(data, "aperture");
	if (cJSON_IsNumber(aperture)) {
		if (aperture->valuedouble >= 0.0) {
			r->scene->camera->aperture = aperture->valuedouble;
		} else {
			r->scene->camera->aperture = 0.0;
		}
	} else {
		logr(warning, "No aperture for camera found");
		return -1;
	}
	
	transforms = cJSON_GetObjectItem(data, "transforms");
	if (cJSON_IsArray(transforms)) {
		int tformCount = cJSON_GetArraySize(transforms);
		addCamTransforms(r->scene->camera, parseTransforms(transforms), tformCount);
	} else {
		logr(warning, "No transforms for camera found");
		return -1;
	}
	
	return 0;
}

struct color *parseColor(const cJSON *data) {
	
	const cJSON *R = NULL;
	const cJSON *G = NULL;
	const cJSON *B = NULL;
	const cJSON *A = NULL;
	
	struct color *newColor = (struct color*)calloc(1, sizeof(struct color));
	
	R = cJSON_GetObjectItem(data, "r");
	if (R != NULL && cJSON_IsNumber(R)) {
		newColor->red = R->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	G = cJSON_GetObjectItem(data, "g");
	if (R != NULL && cJSON_IsNumber(G)) {
		newColor->green = G->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	B = cJSON_GetObjectItem(data, "b");
	if (R != NULL && cJSON_IsNumber(B)) {
		newColor->blue = B->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	A = cJSON_GetObjectItem(data, "a");
	if (R != NULL && cJSON_IsNumber(A)) {
		newColor->alpha = A->valuedouble;
	} else {
		newColor->alpha = 0.0;
	}
	
	return newColor;
}

int parseAmbientColor(struct renderer *r, const cJSON *data) {
	struct color *ambientColor = parseColor(data);
	if (ambientColor != NULL) {
		r->scene->ambientColor = ambientColor;
	} else {
		return -1;
	}
	return 0;
}

void parseOBJ(struct renderer *r, const cJSON *data) {
	const cJSON *fileName = cJSON_GetObjectItem(data, "fileName");
	
	const cJSON *bsdf = cJSON_GetObjectItem(data, "bsdf");
	enum bsdfType type = lambertian;
	
	if (cJSON_IsString(bsdf)) {
		if (strcmp(bsdf->valuestring, "lambertian") == 0) {
			type = lambertian;
		} else if (strcmp(bsdf->valuestring, "metal") == 0) {
			type = metal;
		} else {
			type = lambertian;
		}
	} else {
		logr(warning, "Invalid bsdf while parsing OBJ\n");
	}
	
	bool objValid = false;
	if (fileName != NULL && cJSON_IsString(fileName)) {
		if (loadOBJ(r, fileName->valuestring)) {
			objValid = true;
		} else {
			return;
		}
	}
	if (objValid) {
		const cJSON *transforms = cJSON_GetObjectItem(data, "transforms");
		const cJSON *transform = NULL;
		if (transforms != NULL && cJSON_IsArray(transforms)) {
			cJSON_ArrayForEach(transform, transforms) {
				addTransform(lastObj(r), parseTransform(transform));
			}
		}
		
		for (int i = 0; i < lastObj(r)->materialCount; i++) {
			lastObj(r)->materials[i].type = type;
			assignBSDF(&lastObj(r)->materials[i]);
		}
	}
}

void parseOBJs(struct renderer *r, const cJSON *data) {
	const cJSON *OBJ = NULL;
	if (data != NULL && cJSON_IsArray(data)) {
		cJSON_ArrayForEach(OBJ, data) {
			parseOBJ(r, OBJ);
		}
	}
}

struct vector parseCoordinate(const cJSON *data) {
	const cJSON *X = NULL;
	const cJSON *Y = NULL;
	const cJSON *Z = NULL;
	X = cJSON_GetObjectItem(data, "X");
	Y = cJSON_GetObjectItem(data, "Y");
	Z = cJSON_GetObjectItem(data, "Z");
	
	if (X != NULL && Y != NULL && Z != NULL) {
		if (cJSON_IsNumber(X) && cJSON_IsNumber(Y) && cJSON_IsNumber(Z)) {
			return vectorWithPos(X->valuedouble, Y->valuedouble, Z->valuedouble);
		}
	}
	logr(warning, "Invalid coordinate parsed! Returning 0,0,0\n");
	logr(warning, "Faulty JSON: %s\n", cJSON_Print(data));
	return (struct vector){0.0,0.0,0.0};
}

void parseLight(struct renderer *r, const cJSON *data) {
	const cJSON *pos = NULL;
	const cJSON *radius = NULL;
	const cJSON *color = NULL;
	const cJSON *intensity = NULL;
	
	struct vector posValue;
	double radiusValue = 0.0;
	struct color colorValue;
	double intensityValue = 0.0;
	
	pos = cJSON_GetObjectItem(data, "pos");
	if (pos != NULL) {
		posValue = parseCoordinate(pos);
	} else {
		return;
	}
	
	radius = cJSON_GetObjectItem(data, "radius");
	if (radius != NULL && cJSON_IsNumber(radius)) {
		radiusValue = radius->valuedouble;
	} else {
		return;
	}
	
	color = cJSON_GetObjectItem(data, "color");
	if (color != NULL) {
		colorValue = *parseColor(color);
	} else {
		return;
	}
	
	intensity = cJSON_GetObjectItem(data, "intensity");
	if (intensity != NULL && cJSON_IsNumber(intensity)) {
		intensityValue = intensity->valuedouble;
	} else {
		return;
	}
	
	addLight(r->scene, newLight(posValue, radiusValue, colorValue, intensityValue));
}

void parseLights(struct renderer *r, const cJSON *data) {
	const cJSON *light = NULL;
	if (data != NULL && cJSON_IsArray(data)) {
		cJSON_ArrayForEach(light, data) {
			parseLight(r, light);
		}
	}
}

void parseSphere(struct renderer *r, const cJSON *data) {
	const cJSON *pos = NULL;
	const cJSON *color = NULL;
	const cJSON *reflectivity = NULL;
	const cJSON *radius = NULL;
	
	struct vector posValue;
	struct color colorValue;
	double reflectivityValue = 0.0;
	double radiusValue = 0.0;
	
	const cJSON *bsdf = cJSON_GetObjectItem(data, "bsdf");
	enum bsdfType type = lambertian;
	
	if (cJSON_IsString(bsdf)) {
		if (strcmp(bsdf->valuestring, "lambertian") == 0) {
			type = lambertian;
		} else if (strcmp(bsdf->valuestring, "metal") == 0) {
			type = metal;
		} else {
			type = lambertian;
		}
	} else {
		logr(warning, "Invalid bsdf while parsing OBJ\n");
	}
	
	pos = cJSON_GetObjectItem(data, "pos");
	if (pos != NULL) {
		posValue = parseCoordinate(pos);
	} else {
		return;
	}
	
	color = cJSON_GetObjectItem(data, "color");
	if (color != NULL) {
		colorValue = *parseColor(color);
	} else {
		return;
	}
	
	reflectivity = cJSON_GetObjectItem(data, "reflectivity");
	if (reflectivity != NULL && cJSON_IsNumber(reflectivity)) {
		reflectivityValue = reflectivity->valuedouble;
	} else {
		return;
	}
	
	radius = cJSON_GetObjectItem(data, "radius");
	if (radius != NULL && cJSON_IsNumber(radius)) {
		radiusValue = radius->valuedouble;
	} else {
		return;
	}
	addSphere(r->scene, newSphere(posValue, radiusValue, newMaterial(colorValue, reflectivityValue)));
	
	lastSphere(r)->material.type = type;
	assignBSDF(&lastSphere(r)->material);
}

void parseSpheres(struct renderer *r, const cJSON *data) {
	const cJSON *sphere = NULL;
	if (data != NULL && cJSON_IsArray(data)) {
		cJSON_ArrayForEach(sphere, data) {
			parseSphere(r, sphere);
		}
	}
}

int parseScene(struct renderer *r, const cJSON *data) {
	
	const cJSON *filePath = NULL;
	const cJSON *fileName = NULL;
	const cJSON *inputFilePath = NULL;
	const cJSON *count = NULL;
	const cJSON *width = NULL;
	const cJSON *height = NULL;
	const cJSON *fileType = NULL;
	const cJSON *ambientColor = NULL;
	const cJSON *bounces = NULL;
	const cJSON *lights = NULL;
	const cJSON *spheres = NULL;
	const cJSON *OBJs = NULL;
	
	filePath = cJSON_GetObjectItem(data, "outputFilePath");
	if (cJSON_IsString(filePath)) {
		copyString(filePath->valuestring, &r->image->filePath);
	}
	
	fileName = cJSON_GetObjectItem(data, "outputFileName");
	if (cJSON_IsString(fileName)) {
		copyString(fileName->valuestring, &r->image->fileName);
	}
	
	inputFilePath = cJSON_GetObjectItem(data, "inputFilePath");
	if (cJSON_IsString(inputFilePath)) {
		copyString(inputFilePath->valuestring, &r->inputFilePath);
	}
	
	count = cJSON_GetObjectItem(data, "count");
	if (cJSON_IsNumber(count)) {
		if (count->valueint >= 0) {
			r->image->count = count->valueint;
		} else {
			r->image->count = 0;
		}
	}
	
	//FIXME: This is super ugly
	width = cJSON_GetObjectItem(data, "width");
	if (cJSON_IsNumber(width)) {
		if (width->valueint >= 0) {
			r->image->size.width = width->valueint;
#ifdef UI_ENABLED
			r->mainDisplay->width = width->valueint;
#endif
		} else {
			r->image->size.width = 640;
#ifdef UI_ENABLED
			r->mainDisplay->width = 640;
#endif
		}
	}
	
	height = cJSON_GetObjectItem(data, "height");
	if (cJSON_IsNumber(height)) {
		if (height->valueint >= 0) {
			r->image->size.height = height->valueint;
#ifdef UI_ENABLED
			r->mainDisplay->height = height->valueint;
#endif
		} else {
			r->image->size.height = 640;
#ifdef UI_ENABLED
			r->mainDisplay->height = 640;
#endif
		}
	}
	
	fileType = cJSON_GetObjectItem(data, "fileType");
	if (cJSON_IsString(fileType)) {
		if (strcmp(fileType->valuestring, "png") == 0) {
			r->image->fileType = png;
		} else if (strcmp(fileType->valuestring, "bmp") == 0) {
			r->image->fileType = bmp;
		} else {
			r->image->fileType = png;
		}
	}
	
	ambientColor = cJSON_GetObjectItem(data, "ambientColor");
	if (cJSON_IsObject(ambientColor)) {
		if (parseAmbientColor(r, ambientColor) == -1) {
			return -1;
		}
	}
	
	bounces = cJSON_GetObjectItem(data, "bounces");
	if (cJSON_IsNumber(bounces)) {
		if (bounces->valueint >= 0) {
			r->scene->bounces = bounces->valueint;
		} else {
			r->scene->bounces = 0;
		}
	}
	
	lights = cJSON_GetObjectItem(data, "lights");
	if (cJSON_IsArray(lights)) {
		parseLights(r, lights);
	}
	
	spheres = cJSON_GetObjectItem(data, "spheres");
	if (cJSON_IsArray(spheres)) {
		parseSpheres(r, spheres);
	}
	
	OBJs = cJSON_GetObjectItem(data, "OBJs");
	if (cJSON_IsArray(OBJs)) {
		parseOBJs(r, OBJs);
	}
	
	return 0;
}

int parseJSON(struct renderer *r, char *inputFileName) {
	
	/*
	 Note: Since we are freeing the JSON data (and its' pointers) after parsing,
	 we need to *copy* all dynamically allocated strings with the copyString() function.
	 */
	
	char *buf = loadFile(inputFileName);
	
	cJSON *json = cJSON_Parse(buf);
	if (json == NULL) {
		logr(warning, "Failed to parse JSON\n");
		const char *errptr = cJSON_GetErrorPtr();
		if (errptr != NULL) {
			free(buf);
			cJSON_Delete(json);
			logr(warning, "Error before: %s\n", errptr);
			return -2;
		}
	}
	
	const cJSON *renderer = NULL;
	const cJSON *display = NULL;
	const cJSON *camera = NULL;
	const cJSON *scene = NULL;
	
	renderer = cJSON_GetObjectItem(json, "renderer");
	if (renderer != NULL) {
		if (parseRenderer(r, renderer) == -1) {
			logr(warning, "Renderer parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No renderer found\n");
		free(buf);
		return -2;
	}
	
	display = cJSON_GetObjectItem(json, "display");
	if (display != NULL) {
		if (parseDisplay(r, display) == -1) {
			logr(warning, "Display parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No display found\n");
		free(buf);
		return -2;
	}
	
	camera = cJSON_GetObjectItem(json, "camera");
	if (camera != NULL) {
		if (parseCamera(r, camera) == -1) {
			logr(warning, "Camera parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No camera found\n");
		free(buf);
		return -2;
	}
	
	scene = cJSON_GetObjectItem(json, "scene");
	if (scene != NULL) {
		if (parseScene(r, scene) == -1) {
			logr(warning, "Scene parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No scene found\n");
		free(buf);
		return -2;
	}
	
	cJSON_Delete(json);
	free(buf);
	
	return 0;
}

//Load the scene, allocate buffers, etc
void loadScene(struct renderer *r, char *filename) {
	//Build the scene
	switch (parseJSON(r, filename)) {
		case -1:
			logr(error, "Scene builder failed due to previous error.");
			break;
		case 4:
			logr(error, "Scene debug mode enabled, won't render image.");
			break;
		case -2:
			logr(error, "JSON parser failed.");
			break;
		default:
			break;
	}
	
	transformCameraIntoView(r->scene->camera);
	transformMeshes(r->scene);
	computeKDTrees(r->scene);
	printSceneStats(r->scene);
	
	//Alloc threadPaused booleans, one for each thread
	r->threadPaused = (bool*)calloc(r->threadCount, sizeof(bool));
	//Alloc timers, one for each thread
	r->timers = (struct timeval*)calloc(r->threadCount, sizeof(struct timeval));
	
	//Quantize image into renderTiles
	quantizeImage(r);
	
	//Compute the focal length for the camera
	computeFocalLength(r);
	
	//Allocate memory and create array of pixels for image data
	r->image->data = (unsigned char*)calloc(3 * r->image->size.width * r->image->size.height, sizeof(unsigned char));
	if (!r->image->data) {
		logr(error, "Failed to allocate memory for image data.");
	}
	//Allocate memory for render buffer
	//Render buffer is used to store accurate color values for the renderers' internal use
	r->renderBuffer = (double*)calloc(3 * r->image->size.width * r->image->size.height, sizeof(double));
	
	//Allocate memory for render UI buffer
	//This buffer is used for storing UI stuff like currently rendering tile highlights
	r->uiBuffer = (unsigned char*)calloc(4 * r->image->size.width * r->image->size.height, sizeof(unsigned char));
	
	//Alloc memory for pthread_create() args
	r->renderThreadInfo = (struct threadInfo*)calloc(r->threadCount, sizeof(struct threadInfo));
	if (r->renderThreadInfo == NULL) {
		logr(error, "Failed to allocate memory for threadInfo args.\n");
	}
	
	//Print a useful warning to user if the defined tile size results in less renderThreads
	if (r->tileCount < r->threadCount) {
		logr(warning, "WARNING: Rendering with a less than optimal thread count due to large tile size!\n");
		logr(warning, "Reducing thread count from %i to ", r->threadCount);
		r->threadCount = r->tileCount;
		printf("%i\n", r->threadCount);
	}
}

//Copies source over to the destination pointer.
void copyString(const char *source, char **destination) {
	*destination = (char*)malloc(strlen(source) + 1);
	strcpy(*destination, source);
}

/*
 struct color *ambientColor;
 struct crayOBJ *objs;
 struct light *lights;
 struct material *materials;
 struct sphere *spheres;
 struct camera *camera;
 };
 */

//Free scene data
void freeScene(struct world *scene) {
	if (scene->ambientColor) {
		free(scene->ambientColor);
	}
	if (scene->objs) {
		for (int i = 0; i < scene->objCount; i++) {
			freeOBJ(&scene->objs[i]);
		}
		free(scene->objs);
	}
	if (scene->materials) {
		for (int i = 0; i < scene->materialCount; i++) {
			freeMaterial(&scene->materials[i]);
		}
		free(scene->materials);
	}
	if (scene->lights) {
		free(scene->lights);
	}
	if (scene->spheres) {
		free(scene->spheres);
	}
	if (scene->camera) {
		freeCamera(scene->camera);
		free(scene->camera);
	}
}

//For Windows support, we need our own getdelim()
#if defined(_WIN32) || defined(__linux__)
#define	LONG_MAX	2147483647L	/* max signed long */
#endif
#define	SSIZE_MAX	LONG_MAX	/* max value for a ssize_t */
size_t getDelim(char **lineptr, size_t *n, int delimiter, FILE *stream) {
	char *buf, *pos;
	int c;
	size_t bytes;
	
	if (lineptr == NULL || n == NULL) {
		return 0;
	}
	if (stream == NULL) {
		return 0;
	}
	
	/* resize (or allocate) the line buffer if necessary */
	buf = *lineptr;
	if (buf == NULL || *n < 4) {
		buf = (char*)realloc(*lineptr, 128);
		if (buf == NULL) {
			/* ENOMEM */
			return 0;
		}
		*n = 128;
		*lineptr = buf;
	}
	
	/* read characters until delimiter is found, end of file is reached, or an
	 error occurs. */
	bytes = 0;
	pos = buf;
	while ((c = getc(stream)) != -1) {
		if (bytes + 1 >= SSIZE_MAX) {
			return 0;
		}
		bytes++;
		if (bytes >= *n - 1) {
			buf = realloc(*lineptr, *n + 128);
			if (buf == NULL) {
				/* ENOMEM */
				return 0;
			}
			*n += 128;
			pos = buf + bytes - 1;
			*lineptr = buf;
		}
		
		*pos++ = (char) c;
		if (c == delimiter) {
			break;
		}
	}
	
	if (ferror(stream) || (feof(stream) && (bytes == 0))) {
		/* EOF, or an error from getc(). */
		return 0;
	}
	
	*pos = '\0';
	return bytes;
}
