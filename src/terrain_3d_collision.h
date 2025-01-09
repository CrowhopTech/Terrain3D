// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_COLLISION_CLASS_H
#define TERRAIN3D_COLLISION_CLASS_H

#include <godot_cpp/classes/object.hpp>

#include "constants.h"

using namespace godot;

class Terrain3DCollision : public Object {
	GDCLASS(Terrain3DCollision, Object);
	CLASS_NAME();

public: // Constants

private:
	Terrain3D *_terrain = nullptr;

public:
	Terrain3DCollision() {}
	~Terrain3DCollision() {}
	void initialize(Terrain3D *p_terrain);


protected:
};

#endif // TERRAIN3D_COLLISION_CLASS_H