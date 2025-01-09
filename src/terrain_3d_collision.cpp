// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "terrain_3d_collision.h"
#include "logger.h"

///////////////////////////
// Public Functions
///////////////////////////

void Terrain3DCollision::initialize(Terrain3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	}
}

///////////////////////////
// Protected Functions
///////////////////////////

