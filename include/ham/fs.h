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

#ifndef HAM_FS_H
#define HAM_FS_H 1

/**
 * @defgroup HAM_FS Filesystem
 * @ingroup HAM
 * @{
 */

#include "typedefs.h"

HAM_C_API_BEGIN

typedef enum ham_file_open_flags{
	HAM_OPEN_READ = 0x1,
	HAM_OPEN_WRITE = 0x2,

	HAM_OPEN_RDWR = HAM_OPEN_READ | HAM_OPEN_WRITE,
} ham_file_open_flags;

typedef enum ham_file_kind{
	HAM_FILE_REGULAR,
	HAM_FILE_DIRECTORY,
	HAM_FILE_LINK,
	HAM_FILE_UNKNOWN,

	HAM_FILE_KIND_COUNT,
} ham_file_kind;

typedef struct ham_file_info{
	ham_file_kind kind;
	ham_usize size;
	ham_str8 mime;
} ham_file_info;

typedef struct ham_file ham_file;

ham_api bool ham_path_exists_utf8 (ham_str8  path);
ham_api bool ham_path_exists_utf16(ham_str16 path);
ham_api bool ham_path_exists_utf32(ham_str32 path);

ham_api ham_str8 ham_mime_from_mem(ham_usize len, const void *buf);

ham_api bool ham_path_file_info_utf8 (ham_str8  path, ham_file_info *ret);
ham_api bool ham_path_file_info_utf16(ham_str16 path, ham_file_info *ret);
ham_api bool ham_path_file_info_utf32(ham_str32 path, ham_file_info *ret);

ham_api ham_file *ham_file_open_utf8 (ham_str8  path, ham_u32 flags);
ham_api ham_file *ham_file_open_utf16(ham_str16 path, ham_u32 flags);
ham_api ham_file *ham_file_open_utf32(ham_str32 path, ham_u32 flags);

ham_api void ham_file_close(ham_file *file);

ham_api bool ham_file_get_info(const ham_file *file, ham_file_info *ret);

ham_api ham_usize ham_file_read(ham_file *file, void *buf, ham_usize max_len);
ham_api ham_usize ham_file_write(ham_file *file, const void *buf, ham_usize len);
ham_api ham_usize ham_file_seek(ham_file *file, ham_usize off);
ham_api ham_usize ham_file_tell(const ham_file *file);

ham_api void *ham_file_map(ham_file *file, ham_file_open_flags flags, ham_usize from, ham_usize len);
ham_api bool ham_file_unmap(ham_file *file, void *mapping, ham_usize len);

HAM_C_API_END

#ifdef __cplusplus

#include "str_buffer.h"

namespace ham{
	namespace detail{
		template<typename Char>
		constexpr inline auto cpath_exists = utf_conditional_t<
			Char,
			meta::static_fn<ham_path_exists_utf8>,
			meta::static_fn<ham_path_exists_utf16>,
			meta::static_fn<ham_path_exists_utf32>
		>{};

		template<typename Char>
		constexpr inline auto file_ctype_open = utf_conditional_t<
			Char,
			meta::static_fn<ham_file_open_utf8>,
			meta::static_fn<ham_file_open_utf16>,
			meta::static_fn<ham_file_open_utf32>
		>{};
	}

	template<typename Char>
	static inline bool path_exists(const basic_str<Char> &path){ return detail::cpath_exists<Char>(path); }

	template<typename Char>
	static inline bool path_exists(const basic_str_buffer<Char> &path){ return detail::cpath_exists<Char>(path); }

	enum class file_open_flags: std::underlying_type_t<ham_file_open_flags>{
		read = HAM_OPEN_READ,
		write = HAM_OPEN_WRITE,

		rdwr = HAM_OPEN_RDWR,
	};

	class file_open_error: public exception{
		public:
			const char *api() const noexcept override{ return "ham::file::file"; };
			const char *what() const noexcept override{ return "Error in ham_file_open_utf8"; }
	};

	class file{
		public:
			file() = default;

			explicit file(const str8 &path, file_open_flags flags = file_open_flags::rdwr)
				: m_handle(ham_file_open_utf8(path, static_cast<ham_file_open_flags>(flags)))
			{
				if(!m_handle){
					throw file_open_error();
				}
			}

			template<typename Char>
			explicit file(const basic_str<Char> &path, file_open_flags flags = file_open_flags::rdwr)
				: m_handle(detail::file_ctype_open<Char>(path, static_cast<ham_file_open_flags>(flags))){}

			file(file&&) noexcept = default;

			file &operator=(file&&) noexcept = default;

			operator bool() const noexcept{ return (bool)m_handle; }

			ham_file *ptr() noexcept{ return m_handle.get(); }
			const ham_file *ptr() const noexcept{ return m_handle.get(); }

			usize size() const noexcept{
				ham_file_info info_ret;
				if(!ham_file_get_info(m_handle.get(), &info_ret)){
					return (usize)-1;
				}

				return info_ret.size;
			}

			usize read(void *buf, usize max_len) const noexcept{ return ham_file_read(m_handle.get(), buf, max_len); }
			usize write(const void *buf, usize max_len) noexcept{ return ham_file_write(m_handle.get(), buf, max_len); }

			usize seek(usize off) noexcept{ return ham_file_seek(m_handle.get(), off); }
			usize tell() const noexcept{ return ham_file_tell(m_handle.get()); }

		private:
			unique_handle<ham_file*, ham_file_close> m_handle;
	};
}

#endif // __cplusplus

/**
 * @}
 */

#endif // !HAM_FS_H
