/*
 * Ham Runtime
 * Copyright (C) 2022 Keith Hammond
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HAM_TYPESYS_H
#define HAM_TYPESYS_H 1

/**
 * @defgroup HAM_TYPESYS Runtime types
 * @ingroup HAM
 * @{
 */

#include "memory.h"
#include "object.h"

HAM_C_API_BEGIN

typedef struct ham_typeset ham_typeset;

typedef enum ham_type_kind_flag{
	HAM_TYPE_THEORETIC,
	HAM_TYPE_OBJECT,
	HAM_TYPE_STRING,
	HAM_TYPE_NUMERIC,

	HAM_TYPE_RUNTIME, // special types from C or other runtimes

	HAM_TYPE_KIND_FLAG_COUNT,

	HAM_TYPE_KIND_MAX_VALUE = 0x7,
} ham_type_kind_flag;

typedef enum ham_type_info_flag{
	HAM_TYPE_INFO_THEORETIC_VOID = 0x0,
	HAM_TYPE_INFO_THEORETIC_UNIT,
	HAM_TYPE_INFO_THEORETIC_TOP,
	HAM_TYPE_INFO_THEORETIC_BOTTOM,
	HAM_TYPE_INFO_THEORETIC_REF,
	HAM_TYPE_INFO_THEORETIC_OBJECT,

	HAM_TYPE_INFO_OBJECT_POD = 0x0,
	HAM_TYPE_INFO_OBJECT_VIRTUAL,

	HAM_TYPE_INFO_STRING_UTF8  = 0x0,
	HAM_TYPE_INFO_STRING_UTF16,
	HAM_TYPE_INFO_STRING_UTF32,

	HAM_TYPE_INFO_NUMERIC_BOOLEAN = 0x0,
	HAM_TYPE_INFO_NUMERIC_NATURAL,
	HAM_TYPE_INFO_NUMERIC_INTEGER,
	HAM_TYPE_INFO_NUMERIC_RATIONAL,
	HAM_TYPE_INFO_NUMERIC_FLOATING_POINT,
	HAM_TYPE_INFO_NUMERIC_VECTOR,

	HAM_TYPE_INFO_RUNTIME_C = 0x0,

	HAM_TYPE_INFO_MAX_VALUE = 0x3F
} ham_type_info_flag;

#define HAM_TYPE_FLAGS_KIND_SHIFT 0u
#define HAM_TYPE_FLAGS_KIND_MASK HAM_TYPE_KIND_MAX_VALUE

#define HAM_TYPE_FLAGS_INFO_SHIFT (ham_popcnt(HAM_TYPE_FLAGS_KIND_MASK))
#define HAM_TYPE_FLAGS_INFO_MASK (HAM_TYPE_INFO_MAX_VALUE << HAM_TYPE_FLAGS_INFO_SHIFT)

typedef struct ham_type ham_type;

ham_api ham_nothrow const char *ham_type_str(const ham_type *type);

ham_api ham_nothrow ham_u32 ham_type_get_flags(const ham_type *type);

ham_used
ham_constexpr ham_nothrow static inline ham_u32 ham_make_type_flags(ham_type_kind_flag kind, ham_type_info_flag info){
	return
		((kind & HAM_TYPE_FLAGS_KIND_MASK) << HAM_TYPE_FLAGS_KIND_SHIFT) |
		((info & HAM_TYPE_FLAGS_INFO_MASK) << HAM_TYPE_FLAGS_INFO_SHIFT)
	;
}

ham_used
ham_constexpr ham_nothrow static inline ham_type_kind_flag ham_type_flags_kind(ham_u32 flags){
	return (ham_type_kind_flag)((flags & HAM_TYPE_FLAGS_KIND_MASK) >> HAM_TYPE_FLAGS_KIND_SHIFT);
}

ham_used
ham_constexpr ham_nothrow static inline ham_type_info_flag ham_type_flags_info(ham_u32 flags){
	return (ham_type_info_flag)((flags & HAM_TYPE_FLAGS_INFO_MASK) >> HAM_TYPE_FLAGS_INFO_SHIFT);
}

ham_api ham_nothrow const char *ham_type_name(const ham_type *type);

ham_api ham_nothrow ham_usize ham_type_alignment(const ham_type *type);
ham_api ham_nothrow ham_usize ham_type_size(const ham_type *type);

ham_api ham_nothrow const ham_type *ham_type_super(const ham_type *type);

/**
 * @defgroup HAM_TYPESYS_OBJECT Object type introspection
 * @{
 */

ham_nonnull_args(1) ham_used
ham_nothrow static inline bool ham_type_is_object(const ham_type *type){
	const ham_u32 flags = ham_type_get_flags(type);
	return ham_type_flags_kind(flags) == HAM_TYPE_OBJECT;
}

ham_api ham_nothrow const ham_object_vtable *ham_type_vptr(const ham_type *type);

ham_api ham_nothrow ham_usize ham_type_num_members(const ham_type *type);
ham_api ham_nothrow ham_usize ham_type_num_methods(const ham_type *type);

typedef bool(*ham_type_members_iterate_fn)(ham_usize i, ham_str8 name, const ham_type *type, void *user);

ham_api ham_usize ham_type_members_iterate(const ham_type *type, ham_type_members_iterate_fn fn, void *user);

typedef bool(*ham_type_methods_iterate_fn)(ham_usize i, ham_str8 name, ham_usize num_params, const ham_str8 *param_names, const ham_type *const *param_types, void *user);

ham_api ham_usize ham_type_methods_iterate(const ham_type *type, ham_type_methods_iterate_fn fn, void *user);

/**
 * @}
 */

/**
 * @defgroup HAM_TYPESYS_TYPESET Typesets
 * @{
 */

ham_nonnull_args(1)
ham_api ham_typeset *ham_typeset_create_alloc(const ham_allocator *allocator);

ham_used
static inline ham_typeset *ham_typeset_create(){
	return ham_typeset_create_alloc(ham_current_allocator());
}

ham_api void ham_typeset_destroy(ham_typeset *ts);

ham_api ham_nothrow const ham_type *ham_typeset_get(const ham_typeset *ts, ham_str8 name);

ham_api const ham_type *ham_typeset_top(const ham_typeset *ts);
ham_api const ham_type *ham_typeset_bottom(const ham_typeset *ts);

ham_api const ham_type *ham_typeset_void(const ham_typeset *ts);
ham_api const ham_type *ham_typeset_unit(const ham_typeset *ts);
ham_api const ham_type *ham_typeset_ref(const ham_typeset *ts, const ham_type *refed);
ham_api const ham_type *ham_typeset_object(const ham_typeset *ts, ham_str8 name);

ham_api const ham_type *ham_typeset_bool(const ham_typeset *ts);
ham_api const ham_type *ham_typeset_nat(const ham_typeset *ts, ham_usize num_bits);
ham_api const ham_type *ham_typeset_int(const ham_typeset *ts, ham_usize num_bits);
ham_api const ham_type *ham_typeset_rat(const ham_typeset *ts, ham_usize num_bits);
ham_api const ham_type *ham_typeset_float(const ham_typeset *ts, ham_usize num_bits);

ham_api const ham_type *ham_typeset_str(const ham_typeset *ts, ham_str_encoding encoding);

ham_api const ham_type *ham_typeset_vec(const ham_typeset *ts, const ham_type *elem, ham_usize n);

/**
 * @}
 */

/**
 * @defgroup HAM_TYPESYS_BUILDER Type builders
 * @{
 */

typedef struct ham_type_builder ham_type_builder;

ham_api ham_type_builder *ham_type_builder_create_alloc(const ham_allocator *allocator);

static inline ham_type_builder *ham_type_builder_create(){
	return ham_type_builder_create_alloc(ham_current_allocator());
}

ham_api ham_nothrow void ham_type_builder_destroy(ham_type_builder *builder);

ham_api ham_nothrow bool ham_type_builder_reset(ham_type_builder *builder);

ham_api const ham_type *ham_type_builder_instantiate(ham_type_builder *builder, ham_typeset *ts);

ham_api ham_nothrow bool ham_type_builder_set_kind(ham_type_builder *builder, ham_type_kind_flag kind);
ham_api ham_nothrow bool ham_type_builder_set_name(ham_type_builder *builder, ham_str8 name);
ham_api ham_nothrow bool ham_type_builder_set_parent(ham_type_builder *builder, const ham_type *parent_type);
ham_api ham_nothrow bool ham_type_builder_set_vptr(ham_type_builder *builder, const ham_object_vtable *vptr);

ham_api bool ham_type_builder_add_member(ham_type_builder *builder, ham_str8 name, const ham_type *type);
ham_api bool ham_type_builder_add_method(ham_type_builder *builder, ham_str8 name, ham_usize num_params, const ham_str8 *param_names, const ham_type *const *param_types);

/**
 * @}
 */

HAM_C_API_END

#ifdef __cplusplus

#include "meta.hpp"

#include <concepts>

namespace ham{
	template<typename Base = void>
	class basic_type{
		public:
			using pointer = const ham_type*;

			constexpr basic_type(pointer ptr_ = nullptr) noexcept: m_ptr(ptr_){}

			constexpr operator bool() const noexcept{ return !!m_ptr; }
			constexpr operator pointer() const noexcept{ return m_ptr; }

			constexpr bool operator==(basic_type other) const noexcept{ return m_ptr == other.m_ptr; }
			constexpr bool operator!=(basic_type other) const noexcept{ return m_ptr != other.m_ptr; }

			constexpr pointer ptr() const noexcept{ return m_ptr; }

			constexpr operator basic_type<void>() const noexcept
				requires (!std::is_same_v<Base, void>)
			{
				return m_ptr;
			}

			usize alignment() const noexcept{
				if constexpr(std::is_integral_v<Base> || std::is_aggregate_v<Base>){
					return alignof(Base);
				}
				else{
					return ham_type_alignment(m_ptr);
				}
			}

			usize size() const noexcept{
				if constexpr(std::is_integral_v<Base> || std::is_aggregate_v<Base>){
					return sizeof(Base);
				}
				else{
					return ham_type_size(m_ptr);
				}
			}

			str8 name() const noexcept{ return ham_type_name(m_ptr); }

		private:
			pointer m_ptr;
	};

	using type = basic_type<>;
	using object_type = basic_type<ham_object>;

	namespace detail{
		template<usize N> struct sized_nat_helper;
		template<usize N> struct sized_int_helper;
		template<usize N> struct sized_rat_helper;
		template<usize N> struct sized_float_helper;

		template<> struct sized_nat_helper<8>:  id<u8>{};
		template<> struct sized_nat_helper<16>: id<u16>{};
		template<> struct sized_nat_helper<32>: id<u32>{};
		template<> struct sized_nat_helper<64>: id<u64>{};

		template<> struct sized_int_helper<8>:  id<i8>{};
		template<> struct sized_int_helper<16>: id<i16>{};
		template<> struct sized_int_helper<32>: id<i32>{};
		template<> struct sized_int_helper<64>: id<i64>{};

		#ifdef HAM_INT128
		template<> struct sized_nat_helper<128>: id<u128>{};
		template<> struct sized_int_helper<128>: id<i128>{};
		#endif

		template<> struct sized_rat_helper<8>:  id<ham_rat8>{};
		template<> struct sized_rat_helper<16>: id<ham_rat16>{};
		template<> struct sized_rat_helper<32>: id<ham_rat32>{};
		template<> struct sized_rat_helper<64>: id<ham_rat64>{};
		template<> struct sized_rat_helper<128>: id<ham_rat128>{};
		template<> struct sized_rat_helper<256>: id<ham_rat256>{};

		#ifdef HAM_FLOAT16
		template<> struct sized_float_helper<16>: id<f16>{};
		#endif

		template<> struct sized_float_helper<32>: id<f32>{};
		template<> struct sized_float_helper<64>: id<f64>{};

		#ifdef HAM_FLOAT128
		template<> struct sized_float_helper<128>: id<f128>{};
		#endif

		template<usize N>
		using sized_int = typename sized_int_helper<N>::type;

		template<usize N>
		using sized_nat = typename sized_nat_helper<N>::type;

		template<usize N>
		using sized_rat = typename sized_rat_helper<N>::type;

		template<usize N>
		using sized_float = typename sized_float_helper<N>::type;
	}

	template<usize N>
	using nat_type = basic_type<detail::sized_nat<N>>;

	template<usize N>
	using int_type = basic_type<detail::sized_int<N>>;

	template<usize N>
	using rat_type = basic_type<detail::sized_rat<N>>;

	template<usize N>
	using float_type = basic_type<detail::sized_float<N>>;

	template<typename ... Tags>
	class basic_typeset_view{
		public:
			static constexpr bool is_mutable = meta::type_list_contains_v<meta::type_list<Tags...>, mutable_tag>;

			using pointer = std::conditional_t<is_mutable, ham_typeset*, const ham_typeset*>;

			constexpr basic_typeset_view(pointer ptr = nullptr) noexcept: m_ptr(ptr){}

			constexpr operator basic_typeset_view<>() const noexcept
				requires is_mutable
			{
				return {m_ptr};
			}

			constexpr operator bool() const noexcept{ return !!m_ptr; }
			constexpr operator pointer() const noexcept{ return m_ptr; }

			constexpr pointer ptr() const noexcept{ return m_ptr; }

			type get(str8 name) const noexcept{ return ham_typeset_get(m_ptr, name); }

			type get_void() const noexcept{ return ham_typeset_void(m_ptr); }

			basic_type<ham_unit> get_unit() const noexcept{ return ham_typeset_unit(m_ptr); }
			basic_type<bool> get_bool() const noexcept{ return ham_typeset_bool(m_ptr); }

			type get_nat(usize num_bits) const noexcept{ return ham_typeset_nat(m_ptr, num_bits); }
			type get_int(usize num_bits) const noexcept{ return ham_typeset_int(m_ptr, num_bits); }
			type get_rat(usize num_bits) const noexcept{ return ham_typeset_rat(m_ptr, num_bits); }
			type get_float(usize num_bits) const noexcept{ return ham_typeset_float(m_ptr, num_bits); }

			template<std::size_t N>
			nat_type<N> get_nat() const noexcept{ return ham_typeset_nat(m_ptr, N); }

			template<std::size_t N>
			int_type<N> get_int() const noexcept{ return ham_typeset_int(m_ptr, N); }

			template<std::size_t N>
			rat_type<N> get_rat() const noexcept{ return ham_typeset_rat(m_ptr, N); }

			template<std::size_t N>
			float_type<N> get_float() const noexcept{ return ham_typeset_float(m_ptr, N); }

			object_type get_object(str8 name) const noexcept{ return ham_typeset_object(m_ptr, name); }

		private:
			pointer m_ptr;

			template<typename T>
			friend inline basic_type<T> get_type(basic_typeset_view<> ts);
	};

	using typeset_view = basic_typeset_view<mutable_tag>;
	using const_typeset_view = basic_typeset_view<>;

	class typeset{
		public:
			explicit typeset(const ham_allocator *allocator = ham_current_allocator())
				: m_handle(ham_typeset_create_alloc(allocator)){}

			operator typeset_view() noexcept{ return m_handle.get(); }
			operator const_typeset_view() const noexcept{ return m_handle.get(); }

			type get(str8 name) const noexcept{ return ham_typeset_get(m_handle.get(), name); }

			ham_typeset *handle() noexcept{ return m_handle.get(); }
			const ham_typeset *handle() const noexcept{ return m_handle.get(); }

		private:
			unique_handle<ham_typeset*, ham_typeset_destroy> m_handle;
	};

	class type_builder{
		public:
			explicit type_builder(): m_handle(ham_type_builder_create()){}

			type instantiate(typeset_view ts){ return ham_type_builder_instantiate(m_handle.get(), ts); }

			bool set_kind(ham_type_kind_flag kind){ return ham_type_builder_set_kind(m_handle.get(), kind); }
			bool set_name(str8 name){ return ham_type_builder_set_name(m_handle.get(), name); }
			bool set_parent(type parent){ return ham_type_builder_set_parent(m_handle.get(), parent); }
			bool set_vptr(const ham_object_vtable *vptr){ return ham_type_builder_set_vptr(m_handle.get(), vptr); }

			bool add_member(str8 name, type t){ return ham_type_builder_add_member(m_handle.get(), name, t); }

		private:
			unique_handle<ham_type_builder*, ham_type_builder_destroy> m_handle;
	};

	template<typename T>
	concept Rational =
		std::is_same_v<T, ham_rat8> ||
		std::is_same_v<T, ham_rat16> ||
		std::is_same_v<T, ham_rat32> ||
		std::is_same_v<T, ham_rat64> ||
		std::is_same_v<T, ham_rat128> ||
		std::is_same_v<T, ham_rat256>
	;

	namespace detail{
		template<typename T>
		struct rtti_helper{
			static auto get(const_typeset_view ts){ return ts.get(meta::type_name_v<T>).ptr(); }
		};

		template<>
		struct rtti_helper<void>{
			static auto get(const_typeset_view ts){ return ts.get_void(); }
		};

		template<>
		struct rtti_helper<bool>{
			static auto get(const_typeset_view ts){ return ts.get_bool(); }
		};

		template<std::integral Int>
		struct rtti_helper<Int>{
			static auto get(const_typeset_view ts){
				if constexpr(std::is_signed_v<Int>){
					return ts.get_int<sizeof(Int) * CHAR_BIT>();
				}
				else{
					return ts.get_nat<sizeof(Int) * CHAR_BIT>();
				}
			}
		};

		template<Rational Rat>
		struct rtti_helper<Rat>{
			static auto get(const_typeset_view ts){ return ts.get_rat<sizeof(Rat) * CHAR_BIT>(); }
		};

		template<std::floating_point Float>
		struct rtti_helper<Float>{
			static auto get(const_typeset_view ts){ return ts.get_float<sizeof(Float) * CHAR_BIT>(); }
		};
	}

	template<typename T>
	inline basic_type<T> get_type(const_typeset_view ts){
		return detail::rtti_helper<T>::get(ts);
	}
}

#endif // __cplusplus

/**
 * @}
 */

#endif // !HAM_TYPESYS_H
