//
//  color.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 28/02/15.
//  Copyright (c) 2015-2018 Valtteri Koskivuori. All rights reserved.
//

#include "includes.h"
#include "color.h"

//Some standard colours
struct color redColor =   {1.0, 0.0, 0.0, 0.0};
struct color greenColor = {0.0, 1.0, 0.0, 0.0};
struct color blueColor =  {0.0, 0.0, 1.0, 0.0};
struct color blackColor = {0.0, 0.0, 0.0, 0.0};
struct color whiteColor = {1.0, 1.0, 1.0, 0.0};
struct color frameColor = {1.0, 0.5, 0.0, 0.0};

//Color functions
//Return a color with given values
struct color colorWithValues(double red, double green, double blue, double alpha) {
	return (struct color){red, green, blue, alpha};
}

struct color colorWithRGBAValues(int R, int G, int B, int A) {
	return (struct color){R / 255.0, G / 255.0, B / 255.0, A / 255.0};
}

//Multiply two colors
struct color multiplyColors(struct color *c1, struct color *c2) {
	return (struct color){c1->red * c2->red, c1->green * c2->green, c1->blue * c2->blue, 0.0};
}

//Add two colors
struct color addColors(struct color *c1, struct color *c2) {
	return (struct color){c1->red + c2->red, c1->green + c2->green, c1->blue + c2->blue, 0.0};
}

//Multiply a color with a coefficient value
struct color colorCoef(double coef, struct color *c) {
	return (struct color){c->red * coef, c->green * coef, c->blue * coef, 0.0};
}

/*float* add(const float* v1, const float* v2, unsigned int n) {
 float* v = (float*)calloc(n, sizeof(float));
 for (unsigned int i = 0; i < n; ++i) {
 v[i] = v1[i]+v2[i];
 }
 return v;
 }
 
 float* muls(const float* v, float s, unsigned int n) {
 float* r = (float*)calloc(n, sizeof(float));
 for (unsigned int i = 0; i < n; ++i) {
 r[i] = v[i]*s;
 }
 return r;
 }
 
 float* mix(const float* v1, const float* v2, float a, unsigned int n) {
 return add(muls(v1,1.0f-a,n), muls(v2,a,n), n);
 }*/

/*float addf(const float v1, const float v2) {
	float v = 0.0f;
	v = v1 + v2;
	return v;
}

float mulsf(const float v, float s) {
	float r = 0.0f;
	r = v * s;
	return r;
}

float mixf(const float v1, const float v2, float a) {
	return add(muls(v1,1.0f-a), muls(v2,a));
}*/

struct color add(struct color c1, struct color c2) {
	struct color result = (struct color){0, 0, 0, 0};
	result.red = c1.red + c2.red;
	result.green = c1.green + c2.green;
	result.blue = c1.blue + c2.blue;
	result.alpha = c1.alpha + c2.alpha;
	return result;
}

struct color muls(struct color c, float coeff) {
	struct color result = (struct color){0, 0, 0, 0};
	result.red = c.red * coeff;
	result.green = c.green * coeff;
	result.blue = c.blue * coeff;
	result.alpha = c.alpha * coeff;
	return result;
}

struct color mixColors(struct color c1, struct color c2, float coeff) {
	//Linear interpolation mix
	return add(muls(c1, 1.0f - coeff), muls(c2, coeff));
}

//sRGB transforms are from https://en.wikipedia.org/wiki/SRGB

double linearToSRGB(double channel) {
	if (channel <= 0.0031308) {
		return 12.92 * channel;
	} else {
		return (1.055 * pow(channel, 0.4166666667)) - 0.055;
	}
}

double SRGBToLinear(double channel) {
	if (channel <= 0.04045) {
		return channel / 12.92;
	} else {
		return pow(((channel + 0.055) / 1.055), 2.4);
	}
}

//Convert from linear (0.0-1.0) to sRGB
struct color toSRGB(struct color c) {
	struct color srgb = (struct color){0.0, 0.0, 0.0, 0.0};
	srgb.red = linearToSRGB(c.red);
	srgb.green = linearToSRGB(c.green);
	srgb.blue = linearToSRGB(c.blue);
	srgb.alpha = c.alpha;
	return srgb;
}

//Convert from sRGB to linear (0.0-1.0)
struct color fromSRGB(struct color c) {
	struct color linear = (struct color){0.0, 0.0, 0.0, 0.0};
	linear.red = SRGBToLinear(c.red);
	linear.green = SRGBToLinear(c.green);
	linear.blue = SRGBToLinear(c.blue);
	linear.alpha = c.alpha;
	return linear;
}
