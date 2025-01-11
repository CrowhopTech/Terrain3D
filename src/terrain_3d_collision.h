// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_COLLISION_CLASS_H
#define TERRAIN3D_COLLISION_CLASS_H

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

#include "constants.h"

using namespace godot;

class Terrain3D;

class Terrain3DCollision : public Object {
	GDCLASS(Terrain3DCollision, Object);
	CLASS_NAME();

public: // Constants
	enum CollisionMode {
		DISABLED,
		DYNAMIC_GAME,
		DYNAMIC_EDITOR,
		FULL_GAME,
		FULL_EDITOR,
	};

private:
	Terrain3D *_terrain = nullptr;

	bool _initialized = false;
	CollisionMode _mode = DYNAMIC_GAME;
	uint32_t _shape_size = 16;
	real_t _distance = 64.f;
	uint32_t _layer = 1;
	uint32_t _mask = 1;
	real_t _priority = 1.f;

	uint32_t _shape_width = 0;
	uint32_t _shape_count = 0;
	Array _active_shapes;
	Array _old_shapes;
	Array _inactive_shapes;
	Vector2i _old_snapped_pos;

	StaticBody3D *_body = nullptr;

	CollisionShape3D *_create_shape();
	void _destroy_shape(const CollisionShape3D &p_shape);
	void _mold_shape(CollisionShape3D &p_shape, const Vector3 &p_position);
	Vector2i _snap_position(Vector3 p_position);
	void _move_shape(const CollisionShape3D &p_shape, const Vector3 &p_position);

public:
	Terrain3DCollision() {}
	~Terrain3DCollision() { destroy(); }
	void initialize(Terrain3D *p_terrain);

	void build();
	void update(const Vector3 p_cam_pos = V3_ZERO); // snap?
	void destroy();

	void set_mode(const CollisionMode p_mode);
	CollisionMode get_mode() const { return _mode; }
	bool is_enabled() const { return _mode > DISABLED; }
	bool is_editor_mode() const { return _mode == DYNAMIC_EDITOR || _mode == FULL_EDITOR; }
	bool is_dynamic_mode() const { return _mode == DYNAMIC_GAME || _mode == DYNAMIC_EDITOR; }
	void set_shape_size(const uint32_t p_size);
	uint32_t get_shape_size() const { return _shape_size; }
	void set_distance(const real_t p_distance);
	real_t get_distance() const { return _distance; }
	void set_layer(const uint32_t p_layers);
	uint32_t get_layer() const { return _layer; };
	void set_mask(const uint32_t p_mask);
	uint32_t get_mask() const { return _mask; };
	void set_priority(const real_t p_priority);
	real_t get_priority() const { return _priority; }
	RID get_rid() const;

protected:
	void _bind_methods();
};

typedef Terrain3DCollision::CollisionMode CollisionMode;
VARIANT_ENUM_CAST(Terrain3DCollision::CollisionMode);

#endif // TERRAIN3D_COLLISION_CLASS_H