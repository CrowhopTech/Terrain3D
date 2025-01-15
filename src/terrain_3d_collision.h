// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_COLLISION_CLASS_H
#define TERRAIN3D_COLLISION_CLASS_H

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

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

	// Public settings
	CollisionMode _mode = DYNAMIC_GAME;
	uint16_t _shape_size = 16; // 8 to 64-256, multiples of 4/8?
	uint16_t _radius = 64; // 8 to 256, multiples of 4/8/_shape_size?
	uint32_t _layer = 1;
	uint32_t _mask = 1;
	real_t _priority = 1.f;

	// Work data
	RID _static_body_rid; // Physics Server Static Body
	StaticBody3D *_static_body = nullptr; // Editor mode StaticBody3D
	std::vector<RID *> _shape_rids; // All Physics Server CollisionShapes
	std::vector<CollisionShape3D *> _shapes; // All CollisionShape3Ds
	Array _inactive_shape_ids; // index into above arrays

	bool _initialized = false;
	Vector2i _last_snapped_pos = V2I_MAX;
	Vector2i _snap_to_grid(const Vector3 &p_pos);

	void _form_shape(CollisionShape3D *p_shape, const Vector3 &p_position);
public:
	Terrain3DCollision() {}
	~Terrain3DCollision() { destroy(); }
	void initialize(Terrain3D *p_terrain);

	void build();
	void update(const Vector3 &p_cam_pos = V3_ZERO); // snap?
	void destroy();

	void set_mode(const CollisionMode p_mode);
	CollisionMode get_mode() const { return _mode; }
	bool is_enabled() const { return _mode > DISABLED; }
	bool is_editor_mode() const { return _mode == DYNAMIC_EDITOR || _mode == FULL_EDITOR; }
	bool is_dynamic_mode() const { return _mode == DYNAMIC_GAME || _mode == DYNAMIC_EDITOR; }

	void set_shape_size(const uint32_t p_size);
	uint32_t get_shape_size() const { return _shape_size; }
	void set_radius(const uint16_t p_radius);
	real_t get_radius() const { return _radius; }
	void set_layer(const uint32_t p_layers);
	uint32_t get_layer() const { return _layer; };
	void set_mask(const uint32_t p_mask);
	uint32_t get_mask() const { return _mask; };
	void set_priority(const real_t p_priority);
	real_t get_priority() const { return _priority; }
	RID get_rid() const;

protected:
	static void _bind_methods();
};

typedef Terrain3DCollision::CollisionMode CollisionMode;
VARIANT_ENUM_CAST(Terrain3DCollision::CollisionMode);

inline Vector2i Terrain3DCollision::_snap_to_grid(const Vector3 &p_pos) {
	return Vector2i(
				   Math::floor(p_pos.x / real_t(_shape_size) + 0.5),
				   Math::floor(p_pos.z / real_t(_shape_size) + 0.5)) *
			_shape_size;
}

#endif // TERRAIN3D_COLLISION_CLASS_H