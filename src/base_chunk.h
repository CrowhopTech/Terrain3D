// Copyright © 2024 Lorenz Wildberg

#ifndef BASECHUNK_CLASS_H
#define BASECHUNK_CLASS_H

#include <godot_cpp/templates/vector.hpp>

#include "chunk_manager.h"
#include "constants.h"

using namespace godot;

class ChunkManager;

class BaseChunk : public Object {
	GDCLASS(BaseChunk, Object);
	CLASS_NAME();

public: // Constants

protected:
	Vector2i _position = Vector2i(0, 0);
	unsigned int _size = 0;
	ChunkManager *_manager = nullptr;

public:
	virtual void refill() {}
	virtual void set_enabled(bool enabled) {}
	virtual void set_position(Vector2i p_position) { _position = p_position; };
	Vector2i get_position() { return _position; }

	BaseChunk() {}
	BaseChunk(ChunkManager *p_manager, unsigned int p_size) {
		_manager = p_manager;
		_size = p_size;
	}

protected:
	static void _bind_methods() {}
};

#endif
