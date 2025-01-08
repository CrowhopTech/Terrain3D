// Copyright © 2024 Lorenz Wildberg

#ifndef EDITORCOLLISIONCHUNKMANAGER_CLASS_H
#define EDITORCOLLISIONCHUNKMANAGER_CLASS_H

#include "chunk_manager.h"
#include "collision_chunk_manager.h"
#include <godot_cpp/classes/static_body3d.hpp>

using namespace godot;

class EditorCollisionChunkManager : public CollisionChunkManager {
	GDCLASS(EditorCollisionChunkManager, CollisionChunkManager);
	CLASS_NAME();

public: // Constants
	StaticBody3D *_body = nullptr;

public:
	EditorCollisionChunkManager();
	~EditorCollisionChunkManager();

	BaseChunk *create_chunk() override;

protected:
	static void _bind_methods() {}
};

#endif
