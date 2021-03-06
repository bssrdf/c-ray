//
//  includes.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 28/02/15.
//  Copyright (c) 2015-2018 Valtteri Koskivuori. All rights reserved.
//

#pragma once

//For Xcode: Uncomment this to enable SDL
//#define UI_ENABLED

//Global constants
#define MAX_CRAY_VERTEX_COUNT 3
#define PIOVER180 0.017453292519943295769236907684886
#define PI        3.141592653589793238462643383279502
#define CRAY_MATERIAL_NAME_SIZE 256
#define CRAY_OBJ_FILENAME_LENGTH 500

//Some macros
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define invsqrt(x) (1.0 / sqrt(x))

//Master include file
#ifdef __linux__
#define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>
#include <string.h>

#ifndef WINDOWS
#include <sys/time.h> //for gettimeofday()
#endif

#ifdef WINDOWS
#include <Windows.h>
#include <stdint.h>
#ifdef UI_ENABLED
#include "SDL.h"
#endif
#else
#include <pthread.h>
#ifdef UI_ENABLED
#include <SDL2/SDL.h>
#endif
#endif
#include "lodepng.h"

//These are for multi-platform physical core detection
#ifdef __APPLE__
#include <sys/param.h>
#include <sys/sysctl.h>
#elif _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#endif

//Project includes
#include "color.h"
#include "material.h"
#include "vector.h"
#include "poly.h"
#include "list.h"
#include "obj.h"
#include "sphere.h"
#include "transforms.h"
#include "lightRay.h"

enum fileMode {
	loadModeNormal,//Load scene def normally
	loadModeTarga, //Load previous render state
	saveModeNormal,//Save image
	saveModeTarga, //Save current render state
	saveModeNone   //Don't save
};
