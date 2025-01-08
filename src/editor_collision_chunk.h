// Copyright © 2024 Lorenz Wildberg

#ifndef EDITORCOLLISIONCHUNK_CLASS_H
#define EDITORCOLLISIONCHUNK_CLASS_H

#include <godot_cpp/classes/collision_shape3d.hpp>

#include "base_chunk.h"
#include "editor_collision_chunk_manager.h"

using namespace godot;

class EditorCollisionChunk : public BaseChunk {
	GDCLASS(EditorCollisionChunk, BaseChunk);
	CLASS_NAME();

public: // Constants

private:
	CollisionShape3D *_col_shape = nullptr;

public:
	EditorCollisionChunk() {}
	EditorCollisionChunk(EditorCollisionChunkManager *p_manager, unsigned int p_size);
	~EditorCollisionChunk();

	void refill() override;
	inline PackedFloat32Array fill_map(Transform3D *xform);
	void set_enabled(bool enabled) override;
	void set_position(Vector2i p_position) override;

protected:
	static void _bind_methods() {}
};

#endif
