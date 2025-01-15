// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>

#include "logger.h"
#include "terrain_3d_collision.h"

///////////////////////////
// Private Functions
///////////////////////////

CollisionShape3D *Terrain3DCollision::_create_shape() {
	CollisionShape3D *col_shape = memnew(CollisionShape3D);
	col_shape->set_name("CollisionShape3D");
	col_shape->set_visible(false);
	Ref<HeightMapShape3D> hshape;
	hshape.instantiate();
	hshape->set_map_width(_shape_size + 1);
	hshape->set_map_depth(_shape_size + 1);
	col_shape->set_shape(hshape);
	_body->add_child(col_shape);
	col_shape->set_owner(_body);
	LOG(DEBUG, "Created new shape");
}

void Terrain3DCollision::_destroy_shape(const CollisionShape3D &p_shape) {
	remove_from_tree(p_shape);
	memdelete_safely(p_shape);
}

void Terrain3DCollision::_form_shape(CollisionShape3D &p_shape, const Vector3 &p_position) {
	IS_DATA_INIT_MESG("Terrain not initialized", VOID);
	Ref<Terrain3DData> data = _terrain->get_data();
	int region_size = _terrain->get_region_size();
	real_t vertex_spacing = _terrain->get_vertex_spacing();

	PackedFloat32Array map_data = PackedFloat32Array();
	map_data.resize(_shape_size * _shape_size);

	Ref<Image> map, map_x, map_z, map_xz;
	Ref<Image> cmap, cmap_x, cmap_z, cmap_xz;

	Ref<Terrain3DRegion> region = data->get_regionp(Vector3(p_position.x, 0, p_position.y));
	if (region.is_null()) {
		LOG(ERROR, "Region ", region_loc, " not found");
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

			int shape_global_x = x + (p_position.x % region_size);
			if (shape_global_x < 0) {
				shape_global_x = region_size + shape_global_x;
			}
			LOG(INFO, shape_global_x, " ", p_position.x, " ", region_size, " ", x);
			int shape_global_z = z + (p_position.y % region_size);
			if (shape_global_z < 0) {
				shape_global_z = region_size + shape_global_z;
			}
			// Set heights on local map, or adjacent maps if on the last row/col
			if (shape_global_x < region_size && shape_global_z < region_size) {
				if (map.is_valid()) {
					map_data[index] = (is_hole(cmap->get_pixel(shape_global_x, shape_global_z).r)) ? NAN : map->get_pixel(shape_global_x, shape_global_z).r;
				} else {
					map_data[index] = 0.0;
				}
			} else if (shape_global_x == region_size && shape_global_z < region_size) {
				if (map_x.is_valid()) {
					map_data[index] = (is_hole(cmap_x->get_pixel(0, shape_global_z).r)) ? NAN : map_x->get_pixel(0, shape_global_z).r;
				} else {
					map_data[index] = 0.0f;
				}
			} else if (shape_global_z == region_size && shape_global_x < region_size) {
				if (map_z.is_valid()) {
					map_data[index] = (is_hole(cmap_z->get_pixel(shape_global_x, 0).r)) ? NAN : map_z->get_pixel(shape_global_x, 0).r;
				} else {
					map_data[index] = 0.0f;
				}
			} else if (shape_global_x == region_size && shape_global_z == region_size) {
				if (map_xz.is_valid()) {
					map_data[index] = (is_hole(cmap_xz->get_pixel(0, 0).r)) ? NAN : map_xz->get_pixel(0, 0).r;
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
	p_shape.set_transform(xform);
	Ref<HeightMapShape3D> shape = p_shape.get_shape();
	shape->set_map_data(map_data);
}

Vector2i Terrain3DCollision::_snap_position(Vector3 p_position) {
	Vector2 camera_position = Vector2(p_position.x, p_position.z);
	Vector2i pos_snapped;
	float positive_camera_position_x = (camera_position.x < 0.0) ? -camera_position.x : camera_position.x;
	pos_snapped.x = (Math::floor(positive_camera_position_x / _shape_size) + 0.5) * _shape_size;
	if (camera_position.x < 0.0) {
		pos_snapped.x *= -1;
	}
	float positive_camera_position_y = (camera_position.y < 0.0) ? -camera_position.y : camera_position.y;
	pos_snapped.y = (Math::floor(positive_camera_position_y / _shape_size) + 0.5) * _shape_size;
	if (camera_position.y < 0.0) {
		pos_snapped.y *= -1;
	}
	return pos_snapped;
}

void Terrain3DCollision::_move_shape(const CollisionShape3D &p_shape, const Vector3 &p_camera_position) {
	Vector2i pos_snapped = _snap_position(p_camera_position);
	Vector2i snapped_delta = pos_snapped - _old_snapped_pos;
	_old_snapped_pos = pos_snapped;

	Array new_array = Array();
	new_array.resize(_shape_count);
	new_array.fill(Variant(1));

	// 3 times:
	for (int t = 1; t <= 3; t++) {
		for (int i = 0; i < _shape_width; i++) {
			for (int j = 0; j < _shape_width; j++) {
				int index = i * _shape_width + j;

				// offset to the old camera position
				Vector2i old_array_position = Vector2i(i, j) + snapped_delta;

				// index for the current chunk in the new array in the old array
				int old_index = old_array_position.x * _shape_width + old_array_position.y;

				bool position_still_exists_in_old =
						_shape_width > old_array_position.x && old_array_position.x >= 0 &&
						_shape_width > old_array_position.y && old_array_position.y >= 0;

				// in world coordinates
				Vector2i chunk_location = pos_snapped + Vector2i((i - _shape_width * 0.5) * _shape_size, (j - _shape_width * 0.5) * _shape_size);

				bool too_far = Vector2(chunk_location).distance_to(Vector2(p_camera_position.x, p_camera_position.z)) > _radius;

				switch (t) {
					case 1:
						if (position_still_exists_in_old && !too_far) {
							// move chunk to new location but don't refill
							new_array[index] = _active_shapes[old_index];
							if (_active_shapes[old_index] == Variant(1)) {
								new_array[index] = Object::cast_to<BaseChunk>(_inactive_shapes.pop_back());
								Object::cast_to<BaseChunk>(new_array[index])->set_enabled(true);
							}
							Object::cast_to<BaseChunk>(new_array[index])->set_position(chunk_location);
							_active_shapes[old_index] = Variant(1);
						}
						break;
					case 2:
						// disable remaining chunks
						if (_active_shapes[index] != Variant(1)) {
							Object::cast_to<BaseChunk>(_active_shapes[index])->set_enabled(false);
							_inactive_shapes.push_back(_active_shapes[index]);
							_active_shapes[index] = Variant(1);
						}
						break;
					case 3:
						if (!too_far && !position_still_exists_in_old) {
							// create new chunks from inactive
							BaseChunk *new_chunk = Object::cast_to<BaseChunk>(_inactive_shapes.pop_back());
							new_chunk->set_position(chunk_location);
							new_chunk->refill();
							Transform3D xform;
							PackedFloat32Array map_data = fill_map(&xform);

							Ref<HeightMapShape3D> hshape = _col_shape->get_shape();
							hshape->set_map_data(map_data);
							_col_shape->set_global_transform(xform);
							new_chunk->set_enabled(true);
							new_array[index] = new_chunk;
						}
						break;
				}
			}
		}
	}

	_active_shapes = new_array;
}

///////////////////////////
// Public Functions
///////////////////////////

void Terrain3DCollision::initialize(Terrain3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	}
	if (!IS_EDITOR && _mode != DYNAMIC_GAME) {
		LOG(WARN, "Change collision mode to `DYNAMIC_GAME` for releases");
	}
	build();
}

void Terrain3DCollision::build() {
	IS_DATA_INIT_MESG("Terrain3D not initialized.", VOID);

	// Clear collision as the user might change modes in the editor
	destroy();

	// Create collision only in applicable modes
	if (!is_enabled() || (IS_EDITOR && !is_editor_mode())) {
		return;
	}

	_body = memnew(StaticBody3D);
	_terrain->add_child(_body, true);
	_body->set_owner(_terrain);

	_active_shapes.resize(_shape_count);
	// smaller sizes might be possible
	_inactive_shapes.resize(_shape_count * 2);
	for (int i = 0; i < _shape_count; i++) {
		_inactive_shapes[i] = _create_shape();
		_inactive_shapes[_shape_count + i] = _create_shape();
		_active_shapes[i] = Variant(1);
	}

	_initialized = true;

	// TODO Get snap position instead
	Vector3 cam_pos = (_terrain->get_camera() != nullptr) ? _terrain->get_camera()->get_global_position() : Vector3();
	update(cam_pos);
}

void Terrain3DCollision::update(const Vector3 &p_cam_pos) {
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
	_active_shapes.clear();
	_inactive_shapes.clear();

	remove_from_tree(_body);
	memdelete_safely(_body);
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

void Terrain3DCollision::set_shape_size(const uint32_t p_size) {
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
	_shape_size = size;
	_initialized = false;
	if (size > _radius) {
		set_radius(size);
	} else {
		build();
	}
}

void Terrain3DCollision::set_radius(const real_t p_radius) {
	real_t radius = MAX(_shape_size, p_radius);
	radius = CLAMP(radius, 24.0, 256);
	LOG(INFO, "Setting collision dynamic radius: ", radius);
	_radius = radius;

	_shape_width = ceil((_radius + 1.0) / _shape_size) * 2.0;
	_shape_count = _shape_width * _shape_width;

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
