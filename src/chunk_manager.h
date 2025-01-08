// Copyright © 2024 Lorenz Wildberg

#ifndef CHUNKMANAGER_CLASS_H
#define CHUNKMANAGER_CLASS_H

#include <godot_cpp/classes/node3d.hpp>

#include "base_chunk.h"
#include "constants.h"


using namespace godot;

class BaseChunk;

class ChunkManager : public Node3D {
	GDCLASS(ChunkManager, Node3D);
	CLASS_NAME();

public: // Constants


protected:
	unsigned int _chunk_size = 16;

private:
	float _distance = 64.0;
	unsigned int _chunks_width = 0;
	unsigned int _chunk_count = 0;
	Array _active_chunks = Array();
	Array _old_chunks = Array();
	Array _inactive_chunks = Array();
	Vector2i _old_snapped_pos = Vector2i(0, 0);

	Vector2i _snap_position(Vector3 p_position);
	void _destroy();
	void _build();

public:
	ChunkManager() {}

	void set_chunk_size(unsigned int p_size);
	unsigned int get_chunk_size() { return _chunk_size; }
	void set_distance(float p_distance);
	float get_distance() { return _distance; }
	void move(Vector3 p_camera_position);

protected:
	virtual BaseChunk *create_chunk() { return nullptr; }
	static void _bind_methods() {}
};

#endif
