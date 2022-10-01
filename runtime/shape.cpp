#include "ham/shape.h"
#include "ham/check.h"
#include "ham/memory.h"

HAM_C_API_BEGIN

struct ham_shape{
	const ham_allocator *allocator;
	ham_shape_kind kind;
	ham_vertex_order vertex_order;
	ham_u32 num_points, num_indices;
	ham_vec3 *verts, *norms;
	ham_vec2 *uvs;
	ham_u32 *indices;
};

static inline ham_shape *ham_shape_create(
	ham_shape_kind kind,
	ham_vertex_order vertex_order,
	ham_u32 num_points,
	const ham_vec3 *verts,
	const ham_vec3 *norms,
	const ham_vec2 *uvs,
	ham_u32 num_indices,
	const ham_u32 *indices
){
	const auto allocator = ham_current_allocator();

	const auto ptr = ham_allocator_new(allocator, ham_shape);
	if(!ptr) return nullptr;

	const auto new_verts = ham_allocator_alloc(allocator, alignof(ham_vec3), sizeof(ham_vec3) * num_points);
	if(!new_verts){
		ham_allocator_delete(allocator, ptr);
		return nullptr;
	}

	const auto new_norms = ham_allocator_alloc(allocator, alignof(ham_vec3), sizeof(ham_vec3) * num_points);
	if(!new_norms){
		ham_allocator_free(allocator, new_verts);
		ham_allocator_delete(allocator, ptr);
		return nullptr;
	}

	const auto new_uvs = ham_allocator_alloc(allocator, alignof(ham_vec2), sizeof(ham_vec2) * num_points);
	if(!new_uvs){
		ham_allocator_free(allocator, new_verts);
		ham_allocator_free(allocator, new_norms);
		ham_allocator_delete(allocator, ptr);
		return nullptr;
	}

	const auto new_indices = ham_allocator_alloc(allocator, alignof(ham_u32), sizeof(ham_u32) * num_indices);
	if(!new_indices){
		ham_allocator_free(allocator, new_verts);
		ham_allocator_free(allocator, new_norms);
		ham_allocator_free(allocator, new_uvs);
		ham_allocator_delete(allocator, ptr);
		return nullptr;
	}

	memcpy(new_verts,   verts,   sizeof(ham_vec3) * num_points);
	memcpy(new_norms,   norms,   sizeof(ham_vec3) * num_points);
	memcpy(new_uvs,     uvs,     sizeof(ham_vec2) * num_points);
	memcpy(new_indices, indices, sizeof(ham_u32)  * num_indices);

	ptr->allocator = allocator;
	ptr->kind = kind;
	ptr->vertex_order = vertex_order;
	ptr->num_points = num_points;
	ptr->num_indices = num_indices;
	ptr->verts   = (ham_vec3*)new_verts;
	ptr->norms   = (ham_vec3*)new_norms;
	ptr->uvs     = (ham_vec2*)new_uvs;
	ptr->indices = (ham_u32*)new_indices;

	return ptr;
}

const ham_shape *ham_shape_unit_square(){
	static ham_vec3 verts[] = {
		{ .data = { -0.5f, -0.5f, 0.f } },
		{ .data = {  0.5f, -0.5f, 0.f } },
		{ .data = {  0.5f,  0.5f, 0.f } },
		{ .data = { -0.5f,  0.5f, 0.f } },
	};

	static ham_vec3 norms[] = {
		{ .data = { 0.f, 0.f, 1.f } },
		{ .data = { 0.f, 0.f, 1.f } },
		{ .data = { 0.f, 0.f, 1.f } },
		{ .data = { 0.f, 0.f, 1.f } },
	};

	static ham_vec2 uvs[] = {
		{ .data = { 0.f, 0.f } },
		{ .data = { 1.f, 0.f } },
		{ .data = { 1.f, 1.f } },
		{ .data = { 0.f, 1.f } },
	};

	static ham_u32 indices[] = {
		0, 1, 2, 3
	};

	static const ham_shape ret{
		.allocator = nullptr,
		.kind = HAM_SHAPE_SQUARE,
		.vertex_order = HAM_VERTEX_TRIANGLE_FAN,
		.num_points = 4,
		.num_indices = 4,
		.verts = verts,
		.norms = norms,
		.uvs = uvs,
		.indices = indices
	};

	return &ret;
}

ham_shape *ham_shape_create_quad(const ham_vec2 *points){
	const ham_vec3 verts[] = {
		ham_vec3{ points[0].x, points[0].y, 0.f },
		ham_vec3{ points[1].x, points[1].y, 0.f },
		ham_vec3{ points[2].x, points[2].y, 0.f },
		ham_vec3{ points[3].x, points[3].y, 0.f },
	};

	constexpr ham_vec2 uvs[] = {
		ham_vec2{ .data = { 0.f, 0.f } },
		ham_vec2{ .data = { 1.f, 0.f } },
		ham_vec2{ .data = { 1.f, 1.f } },
		ham_vec2{ .data = { 0.f, 1.f } },
	};

	constexpr ham_vec3 norms[] = {
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
	};

	constexpr ham_u32 indices[] = { 0, 1, 2, 3 };

	return ham_shape_create(HAM_SHAPE_QUAD, HAM_VERTEX_TRIANGLE_FAN, 4, verts, norms, uvs, 4, indices);
}

ham_shape *ham_shape_create_rect(ham_f32 w, ham_f32 h){
	const ham_f32 hw = w * 0.5f;
	const ham_f32 hh = h * 0.5f;

	const ham_vec3 verts[] = {
		ham_vec3{ -hw, -hh, 0.f },
		ham_vec3{  hw, -hh, 0.f },
		ham_vec3{  hw,  hh, 0.f },
		ham_vec3{ -hw,  hh, 0.f },
	};

	constexpr ham_vec2 uvs[] = {
		ham_vec2{ .data = { 0.f, 0.f } },
		ham_vec2{ .data = { 1.f, 0.f } },
		ham_vec2{ .data = { 1.f, 1.f } },
		ham_vec2{ .data = { 0.f, 1.f } },
	};

	constexpr ham_vec3 norms[] = {
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
	};

	constexpr ham_u32 indices[] = { 0, 1, 2, 3 };

	return ham_shape_create(HAM_SHAPE_RECT, HAM_VERTEX_TRIANGLE_FAN, 4, verts, norms, uvs, 4, indices);
}

ham_shape *ham_shape_create_square(ham_f32 dim){
	const ham_f32 hdim = dim * 0.5f;

	const ham_vec3 verts[] = {
		ham_vec3{ -hdim, -hdim, 0.f },
		ham_vec3{  hdim, -hdim, 0.f },
		ham_vec3{  hdim,  hdim, 0.f },
		ham_vec3{ -hdim,  hdim, 0.f },
	};

	constexpr ham_vec2 uvs[] = {
		ham_vec2{ .data = { 0.f, 0.f } },
		ham_vec2{ .data = { 1.f, 0.f } },
		ham_vec2{ .data = { 1.f, 1.f } },
		ham_vec2{ .data = { 0.f, 1.f } },
	};

	constexpr ham_vec3 norms[] = {
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
		ham_vec3{ .data = { 0.f, 0.f, 1.f } },
	};

	constexpr ham_u32 indices[] = { 0, 1, 2, 3 };

	return ham_shape_create(HAM_SHAPE_SQUARE, HAM_VERTEX_TRIANGLE_FAN, 4, verts, norms, uvs, 4, indices);
}

void ham_shape_destroy(ham_shape *shape){
	if(ham_unlikely(shape == NULL)) return;

	const auto allocator = shape->allocator;

	ham_allocator_free(allocator, shape->verts);
	ham_allocator_free(allocator, shape->norms);
	ham_allocator_free(allocator, shape->uvs);
	ham_allocator_free(allocator, shape->indices);

	ham_allocator_delete(allocator, shape);
}

ham_shape_kind ham_shape_get_kind(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->kind : HAM_SHAPE_KIND_COUNT; }

ham_usize ham_shape_num_points(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->num_points : (ham_usize)-1; }
ham_usize ham_shape_num_indices(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->num_indices : (ham_usize)-1; }

const ham_vec3 *ham_shape_vertices(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->verts : nullptr; }
const ham_vec3 *ham_shape_normals(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->norms : nullptr; }
const ham_vec2 *ham_shape_uvs(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->uvs : nullptr; }

const ham_u32 *ham_shape_indices(const ham_shape *shape){ return ham_check(shape != NULL) ? shape->indices : nullptr; }

HAM_C_API_END