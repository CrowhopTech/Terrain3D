// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/world3d.hpp>

#include "constants.h"
#include "logger.h"
#include "terrain_3d_collision.h"
#include "terrain_3d_util.h"

///////////////////////////
// Private Functions
///////////////////////////

void Terrain3DCollision::_form_shape(CollisionShape3D *p_shape, const Vector3 &p_position) {
	IS_DATA_INIT_MESG("Terrain not initialized", VOID);
	Terrain3DData *data = _terrain->get_data();
	int region_size = _terrain->get_region_size();
	real_t vertex_spacing = _terrain->get_vertex_spacing();

	PackedFloat32Array map_data = PackedFloat32Array();
	map_data.resize(_shape_size * _shape_size);

	Ref<Image> map, map_x, map_z, map_xz;
	Ref<Image> cmap, cmap_x, cmap_z, cmap_xz;

	Ref<Terrain3DRegion> region = data->get_regionp(p_position);
	if (region.is_null()) {
		LOG(ERROR, "Region not found at: ", p_position);
		return;
	}
	map = region->get_map(TYPE_HEIGHT);
	cmap = region->get_map(TYPE_CONTROL);

	region = data->get_regionp(Vector3(p_position.x + _shape_size, 0.f, p_position.y) * vertex_spacing);
	if (region.is_valid()) {
		map_x = region->get_map(TYPE_HEIGHT);
		cmap_x = region->get_map(TYPE_CONTROL);
	}
	region = data->get_regionp(Vector3(p_position.x, 0.f, p_position.y + _shape_size) * vertex_spacing);
	if (region.is_valid()) {
		map_z = region->get_map(TYPE_HEIGHT);
		cmap_z = region->get_map(TYPE_CONTROL);
	}
	region = data->get_regionp(Vector3(p_position.x + _shape_size, 0.f, p_position.y + _shape_size) * vertex_spacing);
	if (region.is_valid()) {
		map_xz = region->get_map(TYPE_HEIGHT);
		cmap_xz = region->get_map(TYPE_CONTROL);
	}

	for (int z = 0; z < _shape_size; z++) {
		for (int x = 0; x < _shape_size; x++) {
			// Choose array indexing to match triangulation of heightmapshape with the mesh
			// https://stackoverflow.com/questions/16684856/rotating-a-2d-pixel-array-by-90-degrees
			// Normal array index rotated Y=0 - shape rotation Y=0 (xform below)
			// int index = z * shape_size + x;
			// Array Index Rotated Y=-90 - must rotate shape Y=+90 (xform below)
			int index = _shape_size - 1 - z + x * _shape_size;

			int shape_global_x = x + fmod(p_position.x, region_size);
			if (shape_global_x < 0) {
				shape_global_x = region_size + shape_global_x;
			}
			LOG(INFO, shape_global_x, " ", p_position.x, " ", region_size, " ", x);
			int shape_global_z = z + fmod(p_position.y, region_size);
			if (shape_global_z < 0) {
				shape_global_z = region_size + shape_global_z;
			}
			// Set heights on local map, or adjacent maps if on the last row/col
			if (shape_global_x < region_size && shape_global_z < region_size) {
				if (map.is_valid()) {
					map_data[index] = is_hole(cmap->get_pixel(shape_global_x, shape_global_z).r) ? NAN : map->get_pixel(shape_global_x, shape_global_z).r;
				} else {
					map_data[index] = 0.0;
				}
			} else if (shape_global_x == region_size && shape_global_z < region_size) {
				if (map_x.is_valid()) {
					map_data[index] = is_hole(cmap_x->get_pixel(0, shape_global_z).r) ? NAN : map_x->get_pixel(0, shape_global_z).r;
				} else {
					map_data[index] = 0.0f;
				}
			} else if (shape_global_z == region_size && shape_global_x < region_size) {
				if (map_z.is_valid()) {
					map_data[index] = is_hole(cmap_z->get_pixel(shape_global_x, 0).r) ? NAN : map_z->get_pixel(shape_global_x, 0).r;
				} else {
					map_data[index] = 0.0f;
				}
			} else if (shape_global_x == region_size && shape_global_z == region_size) {
				if (map_xz.is_valid()) {
					map_data[index] = is_hole(cmap_xz->get_pixel(0, 0).r) ? NAN : map_xz->get_pixel(0, 0).r;
				} else {
					map_data[index] = 0.0f;
				}
			}
		}
	}

	// Non rotated shape for normal array index above
	//Transform3D xform = Transform3D(Basis(), global_pos);
	// Rotated shape Y=90 for -90 rotated array index
	Transform3D xform = Transform3D(Basis(Vector3(0, 1.0, 0), Math_PI * .5), Vector3(p_position.x, 0.0, p_position.y));
	p_shape->set_transform(xform);
	Ref<HeightMapShape3D> shape = p_shape->get_shape();
	shape->set_map_data(map_data);
}


///////////////////////////
// Public Functions
///////////////////////////

void Terrain3DCollision::initialize(Terrain3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	}
	if (!IS_EDITOR && is_editor_mode()) {
		LOG(WARN, "Change collision mode to a non-editor mode for releases");
	}
	build();
}

void Terrain3DCollision::build() {
	IS_DATA_INIT_MESG("Terrain3D not initialized.", VOID);

	// Clear collision as the user might change modes in the editor
	destroy();

	// Build only in applicable modes
	if (!is_enabled() || (IS_EDITOR && !is_editor_mode())) {
		return;
	}

	// Create StaticBody3D
	if (is_editor_mode()) {
		LOG(INFO, "Building editor collision");
		_static_body = memnew(StaticBody3D);
		_static_body->set_name("StaticBody3D");
		_static_body->set_as_top_level(true);
		_terrain->add_child(_static_body, true);
		_static_body->set_owner(_terrain);
		_static_body->set_collision_mask(_mask);
		_static_body->set_collision_layer(_layer);
		_static_body->set_collision_priority(_priority);
	} else {
		LOG(INFO, "Building collision with Physics Server");
		_static_body_rid = PS->body_create();
		PS->body_set_mode(_static_body_rid, PhysicsServer3D::BODY_MODE_STATIC);
		PS->body_set_space(_static_body_rid, _terrain->get_world_3d()->get_space());
		PS->body_attach_object_instance_id(_static_body_rid, get_instance_id());
		PS->body_set_collision_mask(_static_body_rid, _mask);
		PS->body_set_collision_layer(_static_body_rid, _layer);
		PS->body_set_collision_priority(_static_body_rid, _priority);
	}

	// Create CollisionShape3Ds
	int shape_count;
	int shape_size; // One additional vertex so # of panels match _shape_size
	if (is_dynamic_mode()) {
		int grid_width = _radius * 2 / _shape_size;
		grid_width = int_ceil_pow2(grid_width, 4);
		shape_count = grid_width * grid_width;
		shape_size = _shape_size + 1;
	} else {
		shape_count = 1;
		shape_size = _terrain->get_region_size() + 1;
	}
	// Preallocate memory, but not size for push_back()
	if (is_editor_mode()) {
		_shapes.reserve(shape_count);
	} else {
		_shape_rids.reserve(shape_count);
	}
	for (int i = 0; i < shape_count; i++) {
		if (is_editor_mode()) {
			CollisionShape3D *col_shape = memnew(CollisionShape3D);
			col_shape->set_name("CollisionShape3D");
			col_shape->set_disabled(true);
			col_shape->set_visible(false);
			Ref<HeightMapShape3D> hshape;
			hshape.instantiate();
			hshape->set_map_width(shape_size);
			hshape->set_map_depth(shape_size);
			col_shape->set_shape(hshape);
			_shapes.push_back(col_shape);
			_static_body->add_child(col_shape, true);
			col_shape->set_owner(_static_body);
			col_shape->set_global_position(V3_MAX);
		} else {
			RID shape_rid = PS->heightmap_shape_create();
			_shape_rids.push_back(&shape_rid);
			Transform3D t;
			t.origin = V3_MAX;
			PS->body_add_shape(_static_body_rid, shape_rid, t, true);
		}
	}


	_initialized = true;
	update(_terrain->get_snapped_position());
}

void Terrain3DCollision::update(const Vector3 &p_cam_pos) {
	if (!_initialized) {
		return;
	}
	LOG(EXTREME, "Updating collision at ", p_cam_pos);
	int time = Time::get_singleton()->get_ticks_usec();

	Vector3 terrain_pos = _terrain->get_snapped_position();
	real_t spacing = _terrain->get_vertex_spacing();

	if (!is_dynamic_mode()) {
		// calculate full collision and return
	} else {
		// Dynamic collision

		/// Step 1. Define Area as a radius on a grid
		// Could be a radius or a grid
		Vector2i snapped_pos;
		snapped_pos = _snap_to_grid(terrain_pos / spacing);
		// Skip if location hasn't moved to next step
		if ((_last_snapped_pos - snapped_pos).length() < _shape_size) {
			return;
		}
		// Create a 0-N grid w/ offset to center it on snapped_pos
		PackedInt32Array grid_slots;
		int grid_width = _radius * 2 / _shape_size;
		grid_width = int_ceil_pow2(grid_width, 4);
		grid_slots.resize(grid_width * grid_width);
		grid_slots.fill(-1);
		Vector2i grid_offset = -V2I(grid_width / 2); // offset to center of grid
		Vector2i shape_offset = V2I(_shape_size / 2); // offset to top left corner
		/// Step 2. Review all shapes
		// If shape is within area, skip
		// Else, mark unused

		_inactive_shape_ids.clear();

		if (is_editor_mode()) {
			for (int i = 0; i < _shapes.size(); i++) {
				CollisionShape3D *shape = _shapes[i];
				Vector3 global_pos = shape->get_global_position();
				Vector2i shape_pos = _snap_to_grid(global_pos / spacing);
				// shape center
				// Optionally could adjust radius to account for corner (sqrt(_shape_size*2))
				if (global_pos.x < FLT_MAX && shape_pos.distance_to(snapped_pos) <= _radius) {
					// Get index into shape array
					//Vector2i shape_loc = snapped_pos - shape_pos + shape_offset + grid_offset;
					Vector2i shape_loc = (shape_pos - snapped_pos + shape_offset + grid_offset) / _shape_size;

					//Shouldn't trigger because of radius shouldn't be larger than index
					if (shape_loc.x < 0 || shape_loc.y < 0 || (shape_loc.y * _shape_size + shape_loc.x) >= grid_slots.size()) {
						LOG(ERROR, "shape_loc out of bounds: ", shape_loc); // temp check
						continue;
					}
					grid_slots[shape_loc.y * _shape_size + shape_loc.x] = i;

					shape->set_disabled(false); // May already be formed, just enable
					continue;
				} else {
					shape->set_disabled(true);
					_inactive_shape_ids.push_back(i);
				}
			}
		} else {
			for (int i = 0; i < _shape_rids.size(); i++) {
				Vector3 shape_origin = PS->body_get_shape_transform(_static_body_rid, i).origin;
				Vector2i shape_pos = _snap_to_grid(shape_origin / spacing);
				if (shape_pos.distance_to(snapped_pos) <= _radius) {
					PS->body_set_shape_disabled(_static_body_rid, i, false);
					continue;
				} else {
					PS->body_set_shape_disabled(_static_body_rid, i, true);
					_inactive_shape_ids.push_back(i);
				}
			}
		}

		/// Step 3. Review all slots in area
		// If slot is full, skip
		// Else assign shape and form it

		for (int i = 0; i < grid_slots.size(); i++) {
			Vector2i grid_loc(i / _shape_size, i % _shape_size);
			if (grid_slots[i] >= 0) {
				continue;
			} else {
				if (_inactive_shape_ids.size() == 0) {
					LOG(ERROR, "No more unused shapes! Aborting!");
					return;
				}
				int sid = _inactive_shape_ids.pop_back();
				CollisionShape3D *shape = _shapes[sid];
				Vector2i shape_loc = grid_loc * _shape_size - grid_offset - shape_offset + snapped_pos;
				Vector3 shape_pos = v2iv3(shape_loc) * spacing;
				_form_shape(shape, shape_pos);
				shape->set_global_position(shape_pos);
				shape->set_disabled(false);
			}
		_last_snapped_pos = snapped_pos;
	}

	LOG(EXTREME, "Collision update time: ", Time::get_singleton()->get_ticks_usec() - time, " us");
}

void Terrain3DCollision::destroy() {
	_initialized = false;
	_last_snapped_pos = V2I_MAX;

	// Physics Server
	for (int i = 0; i < _shape_rids.size(); i++) {
		LOG(DEBUG, "Freeing CollisionShape RID ", i);
		RID rid = *(_shape_rids[i]);
		PS->free_rid(rid);
	}
	_shape_rids.clear();
	if (_static_body_rid.is_valid()) {
		LOG(DEBUG, "Freeing StaticBody RID");
		PS->free_rid(_static_body_rid);
		_static_body_rid = RID();
	}

	// Scene Tree
	for (int i = 0; i < _shapes.size(); i++) {
		CollisionShape3D *shape = _shapes[i];
		LOG(DEBUG, "Freeing CollisionShape3D ", i, " ", shape->get_name());
		remove_from_tree(shape);
		memdelete_safely(shape);
	}
	_shapes.clear();
	if (_static_body != nullptr) {
		LOG(DEBUG, "Freeing StaticBody3D");
		remove_from_tree(_static_body);
		memdelete_safely(_static_body);
		_static_body = nullptr;
	}
}

void Terrain3DCollision::set_mode(const CollisionMode p_mode) {
	LOG(INFO, "Setting collision mode: ", p_mode);
	if (p_mode != _mode) {
		_mode = p_mode;
		if (is_enabled()) {
			build();
		} else {
			destroy();
		}
	}
}

void Terrain3DCollision::set_shape_size(const uint32_t p_size) {
	int size = CLAMP(size, 8, 256);
	size = int_ceil_pow2(size, 4);
	LOG(INFO, "Setting collision dynamic shape size: ", size);
	_shape_size = size;
	if (size > _radius) {
		set_radius(size);
	} else {
		build();
	}
}

void Terrain3DCollision::set_radius(const uint16_t p_radius) {
	int radius = MAX(_shape_size, p_radius);
	radius = CLAMP(radius, 16, 512);
	radius = int_ceil_pow2(radius, 4);
	LOG(INFO, "Setting collision dynamic radius: ", radius);
	_radius = radius;


	build();
}

void Terrain3DCollision::set_layer(const uint32_t p_layers) {
	LOG(INFO, "Setting collision layers: ", p_layers);
	_layer = p_layers;
	if (is_editor_mode()) {
		if (_static_body != nullptr) {
			_static_body->set_collision_layer(_layer);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PS->body_set_collision_layer(_static_body_rid, _layer);
		}
	}
}

void Terrain3DCollision::set_mask(const uint32_t p_mask) {
	LOG(INFO, "Setting collision mask: ", p_mask);
	_mask = p_mask;
	if (is_editor_mode()) {
		if (_static_body != nullptr) {
			_static_body->set_collision_mask(_mask);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PS->body_set_collision_mask(_static_body_rid, _mask);
		}
	}
}

void Terrain3DCollision::set_priority(const real_t p_priority) {
	LOG(INFO, "Setting collision priority: ", p_priority);
	_priority = p_priority;
	if (is_editor_mode()) {
		if (_static_body != nullptr) {
			_static_body->set_collision_priority(_priority);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PS->body_set_collision_priority(_static_body_rid, _priority);
		}
	}
}

RID Terrain3DCollision::get_rid() const {
	if (!is_editor_mode()) {
		return _static_body_rid;
	} else {
		if (_static_body != nullptr) {
			return _static_body->get_rid();
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

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &Terrain3DCollision::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &Terrain3DCollision::get_mode);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Terrain3DCollision::is_enabled);
	ClassDB::bind_method(D_METHOD("is_editor_mode"), &Terrain3DCollision::is_editor_mode);
	ClassDB::bind_method(D_METHOD("is_dynamic_mode"), &Terrain3DCollision::is_dynamic_mode);

	ClassDB::bind_method(D_METHOD("set_shape_size", "size"), &Terrain3DCollision::set_shape_size);
	ClassDB::bind_method(D_METHOD("get_shape_size"), &Terrain3DCollision::get_shape_size);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &Terrain3DCollision::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &Terrain3DCollision::get_radius);
	ClassDB::bind_method(D_METHOD("set_layer", "layers"), &Terrain3DCollision::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer"), &Terrain3DCollision::get_layer);
	ClassDB::bind_method(D_METHOD("set_mask", "mask"), &Terrain3DCollision::set_mask);
	ClassDB::bind_method(D_METHOD("get_mask"), &Terrain3DCollision::get_mask);
	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &Terrain3DCollision::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &Terrain3DCollision::get_priority);
	ClassDB::bind_method(D_METHOD("get_rid"), &Terrain3DCollision::get_rid);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Disabled, Dynamic / Game,Dynamic / Editor,Full / Game,Full / Editor"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape_size", PROPERTY_HINT_RANGE, "8,256,2"), "set_shape_size", "get_shape_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "24,256,1"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_layer", "get_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_mask", "get_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "priority"), "set_priority", "get_priority");
}
