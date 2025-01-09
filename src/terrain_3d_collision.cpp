// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/collision_shape3d.hpp>

#include "logger.h"
#include "terrain_3d_collision.h"

///////////////////////////
// Public Functions
///////////////////////////

void Terrain3DCollision::initialize(Terrain3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	}
	build();
	if (!IS_EDITOR && _mode != DYNAMIC_GAME) {
		LOG(WARN, "Change collision mode to `DYNAMIC_GAME` for releases");
	}
}

void Terrain3DCollision::build() {
	// Skip if not enabled or not in the world
	if (!_enabled || !_is_inside_world || !is_inside_tree()) {
		return;
	}
	// Clear collision as the user might change modes in the editor
	destroy();

	// Create collision only in applicable modes
	if (IS_EDITOR && !is_editor_mode()) {
		return;
	}
	if (_data == nullptr) {
		LOG(ERROR, "_data not initialized, cannot create collision");
		return;
	}

	_chunk_manager = memnew(EditorCollisionChunkManager);
	_chunk_manager->set_terrain(this);
	_chunk_manager->set_distance(_dynamic_distance);
	_chunk_manager->set_chunk_size(_dynamic_shape_size);
	add_child(_chunk_manager);
	_chunk_manager->set_owner(this);
	_initialized = true;
	// TODO Get snap position instead
	Vector3 cam_pos = (_camera != nullptr) ? _camera->get_global_position() : Vector3();

	update(cam_pos);
}

void Terrain3DCollision::update(Vector3 p_cam_pos) {
	if (!_initialized) {
		return;
	}

	LOG(DEBUG, "Updating collision");

	Vector3 cam_pos = p_cam_pos;
	cam_pos.y = 0;
	int time = Time::get_singleton()->get_ticks_usec();
	_chunk_manager->move(cam_pos);
	LOG(EXTREME, "Collision update time: ", Time::get_singleton()->get_ticks_usec() - time, " us");
}

void Terrain3DCollision::destroy() {
	remove_from_tree(_chunk_manager);
	memdelete_safely(_chunk_manager);
}

void Terrain3DCollision::set_mode(const CollisionMode p_mode) {
	LOG(INFO, "Setting collision mode: ", p_mode);
	if (p_mode != _mode) {
		_mode = p_mode;
		_initialized = false;
		if (is_enabled()) {
			build();
		} else {
			destroy();
		}
	}
}

void Terrain3DCollision::set_dynamic_shape_size(const uint32_t p_size) {
	// Round up to nearest power of 2
	uint32_t size = p_size;
	{
		size--;
		size |= size >> 1;
		size |= size >> 2;
		size |= size >> 4;
		size |= size >> 8;
		size |= size >> 16;
		size++;
	}
	size = CLAMP(size, 8, 256);
	LOG(INFO, "Setting collision dynamic shape size: ", size);
	_dynamic_shape_size = size;
	_initialized = false;
	if (size > _dynamic_distance) {
		set_dynamic_distance(size);
	} else {
		build();
	}
}

void Terrain3DCollision::set_dynamic_distance(const real_t p_distance) {
	p_distance = MAX(_dynamic_shape_size, p_distance);
	p_distance = CLAMP(p_distance, 24.0, 256);
	LOG(INFO, "Setting collision dynamic distance: ", p_distance);
	_dynamic_distance = p_distance;
	_initialized = false;
	build();
}

void Terrain3DCollision::set_layer(const uint32_t p_layers) {
	LOG(INFO, "Setting collision layers: ", p_layers);
	_layer = p_layers;
	if (is_editor_mode()) {
		if (_editor_static_body != nullptr) {
			_editor_static_body->set_collision_layer(_layer);
		}
	} else {
		if (_static_body.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_layer(_static_body, _layer);
		}
	}
}

void Terrain3DCollision::set_mask(const uint32_t p_mask) {
	LOG(INFO, "Setting collision mask: ", p_mask);
	_mask = p_mask;
	if (is_editor_mode()) {
		if (_editor_static_body != nullptr) {
			_editor_static_body->set_collision_mask(_mask);
		}
	} else {
		if (_static_body.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_mask(_static_body, _mask);
		}
	}
}

void Terrain3DCollision::set_priority(const real_t p_priority) {
	LOG(INFO, "Setting collision priority: ", p_priority);
	_priority = p_priority;
	if (is_editor_mode()) {
		if (_editor_static_body != nullptr) {
			_editor_static_body->set_collision_priority(_priority);
		}
	} else {
		if (_static_body.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_priority(_static_body, _priority);
		}
	}
}

RID Terrain3DCollision::get_rid() const {
	if (!is_editor_mode()) {
		return _static_body;
	} else {
		if (_editor_static_body != nullptr) {
			return _editor_static_body->get_rid();
		}
	}
	return RID();
}

///////////////////////////
// Protected Functions
///////////////////////////

void Terrain3DCollision::_bind_methods() {
	BIND_ENUM_CONSTANT(DISABLED);
	BIND_ENUM_CONSTANT(DYNAMIC_GAME);
	BIND_ENUM_CONSTANT(DYNAMIC_EDITOR);
	BIND_ENUM_CONSTANT(FULL_GAME);
	BIND_ENUM_CONSTANT(FULL_EDITOR);
}