// Copyright © 2024 Lorenz Wildberg

#include "editor_collision_chunk_manager.h"
#include "editor_collision_chunk.h"

EditorCollisionChunkManager::EditorCollisionChunkManager() {
	_body = memnew(StaticBody3D);
	add_child(_body);
	_body->set_owner(this);
}

EditorCollisionChunkManager::~EditorCollisionChunkManager() {
	remove_from_tree(_body);
	memdelete_safely(_body);
}

BaseChunk *EditorCollisionChunkManager::create_chunk() {
	EditorCollisionChunk *chunk = memnew(EditorCollisionChunk(this, _chunk_size));
	return chunk;
}
