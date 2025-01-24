// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/world3d.hpp>

#include <godot_cpp/classes/scene_tree.hpp>

#include "constants.h"
#include "logger.h"
#include "terrain_3d_collision.h"
#include "terrain_3d_util.h"

///////////////////////////
// Private Functions
///////////////////////////

// Calculates shape data from top left position. Assumes descaled and snapped.
Dictionary Terrain3DCollision::_get_shape_data(const Vector2i &p_position) {
	IS_DATA_INIT_MESG("Terrain not initialized", Dictionary());
	Terrain3DData *data = _terrain->get_data();
	int region_size = _terrain->get_region_size();

	int hshape_size = _shape_size + 1; // Calculate last vertex at end
	PackedFloat32Array map_data = PackedFloat32Array();
	map_data.resize(hshape_size * hshape_size);
	real_t min_height = FLT_MAX;
	real_t max_height = FLT_MIN;

	Ref<Image> map, map_x, map_z, map_xz; // height maps
	Ref<Image> cmap, cmap_x, cmap_z, cmap_xz; // control maps w/ holes


	// Get region_loc of top left corner of descaled and grid snapped collision shape position
	Vector2i region_loc = data->_get_region_location(p_position);
	Ref<Terrain3DRegion> region = data->get_region(region_loc);
	if (region.is_null()) {
		LOG(DEBUG, "Region not found at: ", region_loc, ". Returning blank");
		return Dictionary();
	}
	map = region->get_map(TYPE_HEIGHT);
	cmap = region->get_map(TYPE_CONTROL);

	// Get +X, +Z adjacent regions in case we run over
	region = data->get_region(region_loc + Vector2i(1, 0));
	if (region.is_valid()) {
		map_x = region->get_map(TYPE_HEIGHT);
		cmap_x = region->get_map(TYPE_CONTROL);
	}
	region = data->get_region(region_loc + Vector2i(0, 1));
	if (region.is_valid()) {
		map_z = region->get_map(TYPE_HEIGHT);
		cmap_z = region->get_map(TYPE_CONTROL);
	}
	region = data->get_region(region_loc + Vector2i(1, 1));
	if (region.is_valid()) {
		map_xz = region->get_map(TYPE_HEIGHT);
		cmap_xz = region->get_map(TYPE_CONTROL);
	}

	for (int z = 0; z < hshape_size; z++) {
		for (int x = 0; x < hshape_size; x++) {
			// Choose array indexing to match triangulation of heightmapshape with the mesh
			// https://stackoverflow.com/questions/16684856/rotating-a-2d-pixel-array-by-90-degrees
			// Normal array index rotated Y=0 - shape rotation Y=0 (xform below)
			// int index = z * hshape_size + x;
			// Array Index Rotated Y=-90 - must rotate shape Y=+90 (xform below)
			int index = hshape_size - 1 - z + x * hshape_size;

			Vector2i shape_pos = p_position + Vector2i(x, z);
			Vector2i shape_region_loc = data->_get_region_location(shape_pos);
			int img_x = Math::posmod(shape_pos.x, region_size);
			bool next_x = shape_region_loc.x > region_loc.x;
			int img_y = Math::posmod(shape_pos.y, region_size);
			bool next_z = shape_region_loc.y > region_loc.y;


			// Set heights on local map, or adjacent maps if on the last row/col
			real_t height = 0.f;
			if (!next_x && !next_z && map.is_valid()) {
				height = is_hole(cmap->get_pixel(img_x, img_y).r) ? NAN : map->get_pixel(img_x, img_y).r;
			} else if (next_x && !next_z && map_x.is_valid()) {
				height = is_hole(cmap_x->get_pixel(img_x, img_y).r) ? NAN : map_x->get_pixel(img_x, img_y).r;
			} else if (!next_x && next_z && map_z.is_valid()) {
				height = is_hole(cmap_z->get_pixel(img_x, img_y).r) ? NAN : map_z->get_pixel(img_x, img_y).r;
			} else if (next_x && next_z && map_xz.is_valid()) {
				height = is_hole(cmap_xz->get_pixel(img_x, img_y).r) ? NAN : map_xz->get_pixel(img_x, img_y).r;
			}

			map_data[index] = height;
			if (!std::isnan(height)) {
				min_height = MIN(min_height, height);
				max_height = MAX(max_height, height);
			}
		}
	}

	// Non rotated shape for normal array index above
	//Transform3D xform = Transform3D(Basis(), global_pos);
	// Rotated shape Y=90 for -90 rotated array index
	Transform3D xform = Transform3D(Basis(Vector3(0, 1.0, 0), Math_PI * .5), v2iv3(p_position + V2(_shape_size / 2)));
	Dictionary shape_data;
	shape_data["width"] = hshape_size;
	shape_data["depth"] = hshape_size;
	shape_data["heights"] = map_data;
	shape_data["xform"] = xform;
	shape_data["min_height"] = min_height;
	shape_data["max_height"] = max_height;
	return shape_data;
}

///////////////////////////
// Public Functions
///////////////////////////

void Terrain3DCollision::initialize(Terrain3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	} else {
		return;
	}
	if (!IS_EDITOR && is_editor_mode()) {
		LOG(WARN, "Change collision mode to a non-editor mode for releases");
	}
	build();
}

void Terrain3DCollision::build() {
	if (_terrain == nullptr) {
		LOG(DEBUG, "Build called before terrain initialized. Returning.");
		return;
	}

	IS_DATA_INIT_MESG("Terrain3D not initialized.", VOID);

	// Clear collision as the user might change modes in the editor
	destroy();

	// TODO Remove
	if (_terrain->has_node("StaticBody3D")) {
		Node *node = _terrain->get_node_internal("StaticBody3D");
		remove_from_tree(node);
	}

	// Build only in applicable modes
	if (!is_enabled() || (IS_EDITOR && !is_editor_mode())) {
		return;
	}

	LOG(MESG, "---- 0. Building collision ----");

	// Create StaticBody3D
	if (is_editor_mode()) {
		LOG(MESG, "Building editor collision");
		_static_body = memnew(StaticBody3D);
		_static_body->set_name("StaticBody3D");
		_static_body->set_as_top_level(true);
		_terrain->add_child(_static_body, true);
		//_static_body->set_owner(_terrain);
		_static_body->set_owner(_terrain->get_tree()->get_edited_scene_root());
		_static_body->set_collision_mask(_mask);
		_static_body->set_collision_layer(_layer);
		_static_body->set_collision_priority(_priority);
	} else {
		LOG(MESG, "Building collision with Physics Server");
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
	int hshape_size;
	if (is_dynamic_mode()) {
		int grid_width = _radius * 2 / _shape_size;
		grid_width = int_ceil_pow2(grid_width, 4);
		shape_count = grid_width * grid_width;
		hshape_size = _shape_size + 1;
		LOG(MESG, "Grid width: ", grid_width);
	} else {
		shape_count = 1;
		hshape_size = _terrain->get_region_size() + 1;
	}
	// make array_size an even number?
	// shape_count = (array_size % 2 == 0) ? array_size : array_size + 1.0;
	// Preallocate memory, but not size for push_back()
	if (is_editor_mode()) {
		_shapes.reserve(shape_count);
	} else {
		_shape_rids.reserve(shape_count);
	}
	LOG(MESG, "Shape count: ", shape_count);
	LOG(MESG, "Shape size: ", _shape_size, ", hshape_size: ", hshape_size);
	for (int i = 0; i < shape_count; i++) {
		if (is_editor_mode()) {
			CollisionShape3D *col_shape = memnew(CollisionShape3D);
			col_shape->set_name("CollisionShape3D");
			col_shape->set_disabled(true);
			col_shape->set_visible(true);
			Ref<HeightMapShape3D> hshape;
			hshape.instantiate();
			hshape->set_map_width(hshape_size);
			hshape->set_map_depth(hshape_size);
			col_shape->set_shape(hshape);
			_shapes.push_back(col_shape);
			_static_body->add_child(col_shape, true);
			//col_shape->set_owner(_static_body);
			col_shape->set_owner(_terrain->get_tree()->get_edited_scene_root());
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

		// Snap descaled position to a _shape_size grid (eg. multiples of 16)
		// spacing correct? Copied from instancer grid
		Vector2i snapped_pos;
		snapped_pos = _snap_to_grid(terrain_pos / spacing);

		// Skip if location hasn't moved to next step
		if ((_last_snapped_pos - snapped_pos).length() < _shape_size) {
			return;
		}

		LOG(MESG, "---- 1. Defining area as a radius on a grid ----");
		// Could be a radius or a grid

		// Create a 0-N grid, center on snapped_pos
		PackedInt32Array grid;
		int grid_width = _radius * 2 / _shape_size; // 64*2/16 = 8
		grid_width = int_ceil_pow2(grid_width, 4);
		grid.resize(grid_width * grid_width);
		grid.fill(-1);
		Vector2i grid_offset = -V2I(grid_width / 2); // offset # cells to center of grid
		Vector2i shape_offset = V2I(_shape_size / 2); // offset meters to top left corner of shape
		Vector2i grid_pos = snapped_pos + grid_offset * _shape_size; // Top left of grid
		LOG(MESG, "New Snapped position: ", snapped_pos);
		LOG(MESG, "Grid_pos: ", grid_pos);
		LOG(MESG, "Radius: ", _radius, ", Grid_width: ", grid_width, ", Grid_offset: ", grid_offset, ", # cells: ", grid.size());
		LOG(MESG, "Shape_size: ", _shape_size, ", shape_offset: ", shape_offset);

		_inactive_shape_ids.clear();

		LOG(MESG, "---- 2. Checking existing shapes ----");
		// If shape is within area, skip
		// Else, mark unused

		if (is_editor_mode()) {
			for (int i = 0; i < _shapes.size(); i++) {
				CollisionShape3D *shape = _shapes[i];
				// Descaled global position of shape center
				Vector3 shape_center = shape->get_global_position() / spacing;
				// Unique key: Top left corner of shape, snapped to grid
				Vector2i shape_pos = _snap_to_grid(v3v2i(shape_center) - shape_offset);
				// Optionally could adjust radius to account for corner (sqrt(_shape_size*2))
				// Spacing on distance?
				if (shape_center.x < FLT_MAX && v3v2i(shape_center).distance_to(snapped_pos) <= real_t(_radius) / spacing) {
					// Get index into shape array
					Vector2i grid_loc = (shape_pos - grid_pos) / _shape_size;
					LOG(MESG, "Shape ", i, ": shape_center: ", shape_center.x < FLT_MAX ? shape_center : V3(-999), ", shape_pos: ", shape_pos,
							", grid_loc: ", grid_loc, ", index: ", (grid_loc.y * grid_width + grid_loc.x), " active");

					//Shouldn't trigger because of radius shouldn't be larger than index
					if (grid_loc.x < 0 || grid_loc.y < 0 || (grid_loc.y * grid_width + grid_loc.x) >= grid.size()) {
						LOG(ERROR, "Shape ", i, ": grid_loc out of bounds: ", grid_loc, " shape_pos: ", shape_pos, " - shouldn't happen!"); // temp check
						_inactive_shape_ids.push_back(i);
						shape->set_disabled(true);
						continue;
					}
					grid[grid_loc.y * grid_width + grid_loc.x] = i;
					shape->set_disabled(false); // May already be formed, just enable
				} else {
					_inactive_shape_ids.push_back(i);
					shape->set_disabled(true);
					LOG(MESG, "Shape ", i, ": shape_center: ", shape_center.x < FLT_MAX ? shape_center : V3(-999), ", shape_pos: ", shape_pos,
							" out of bounds, marking inactive");
				}
			}
		} else {
			// TODO
			for (int i = 0; i < _shape_rids.size(); i++) {
				Vector3 shape_origin = PS->body_get_shape_transform(_static_body_rid, i).origin;
				Vector2i shape_pos = _snap_to_grid(shape_origin / spacing);
				if (shape_pos.distance_to(snapped_pos) <= _radius) {
					//grid[shape_loc.y * _shape_size + shape_loc.x] = i;
					PS->body_set_shape_disabled(_static_body_rid, i, false);
					continue;
				} else {
					//grid[shape_loc.y * _shape_size + shape_loc.x] = -1;
					PS->body_set_shape_disabled(_static_body_rid, i, true);
					_inactive_shape_ids.push_back(i);
				}
			}
		}
		LOG(MESG, "_inactive_shapes size: ", _inactive_shape_ids.size());

		LOG(MESG, "---- 3. Review grid cells in area ----");
		// If cell is full, skip
		// Else assign shape and form it

		for (int i = 0; i < grid.size(); i++) {
			Vector2i grid_loc(i % grid_width, i / grid_width);
			// Unique key: Top left corner of shape, snapped to grid
			Vector2i shape_pos = grid_pos + grid_loc * _shape_size;

			if ((shape_pos + shape_offset).distance_to(snapped_pos) > real_t(_radius) / spacing) {
				LOG(MESG, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " out of circle, skipping");
				continue;
			}

			if (grid[i] >= 0) {
				CollisionShape3D *shape = _shapes[grid[i]];
				Vector2i center_pos = v3v2i(shape->get_global_position());
				LOG(MESG, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " act ", center_pos - shape_offset, " Has active shape id: ", grid[i]);
				continue;
			} else {
				if (_inactive_shape_ids.size() == 0) {
					LOG(ERROR, "No more unused shapes! Aborting!");
					break;
				}
				Dictionary shape_data = _get_shape_data(shape_pos); // *spacing
				if (shape_data.is_empty()) {
					LOG(MESG, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " No region found");
					continue;
				}
				int sid = _inactive_shape_ids.pop_back();
				CollisionShape3D *shape = _shapes[sid];
				Transform3D xform = shape_data["xform"];
				LOG(MESG, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " act ", v3v2i(xform.origin) - shape_offset, " placing shape id ", sid);
				xform.origin *= spacing; // scale only at the end when interfacing w/ the engine
				shape->set_transform(xform);
				shape->set_disabled(false);
				Ref<HeightMapShape3D> hshape = shape->get_shape();
				hshape->set_map_data(shape_data["heights"]);
			}

			/// neeed physics server version
		}

		_last_snapped_pos = snapped_pos;
		LOG(MESG, "Setting _last_snapped_pos: ", _last_snapped_pos);
		LOG(MESG, "_inactive_shape_ids size: ", _inactive_shape_ids.size());
	}

	LOG(EXTREME, "Collision update time: ", Time::get_singleton()->get_ticks_usec() - time, " us");
}

void Terrain3DCollision::destroy() {
	_initialized = false;
	_last_snapped_pos = V2I_MAX;
	_inactive_shape_ids.clear();

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

void Terrain3DCollision::set_shape_size(const uint16_t p_size) {
	int size = CLAMP(p_size, 8, 256);
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
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "16,256,1"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_layer", "get_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_mask", "get_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "priority"), "set_priority", "get_priority");
}
