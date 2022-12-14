#
# Ham World Engine Client
# Copyright (C) 2022 Keith Hammond
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

##
# Dependencies
##

find_package(SDL2 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

##
# Targets
##

ham_add_executable(
	ham-engine-client
	client.h
	vulkan.cpp
	window.cpp
	net.cpp
	video.cpp
	main.cpp
)

set_target_properties(
	ham-engine-client PROPERTIES
	ENABLE_EXPORTS ON
)

target_compile_definitions(ham-engine-client PRIVATE HAM_ENGINE_CLIENT_IMPLEMENTATION)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_link_options(ham-engine-client PRIVATE "-rdynamic")
endif()

target_link_libraries(
	ham-engine-client PRIVATE
	ham::engine
	SDL2::SDL2
)
