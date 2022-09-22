#
# The Ham World Engine
# Copyright (C) 2022  Hamsmith Ltd.
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
# Configuration
##

set(HAM_ENGINE_VERSION_MAJOR ${HAM_VERSION_MAJOR})
set(HAM_ENGINE_VERSION_MINOR ${HAM_VERSION_MINOR})
set(HAM_ENGINE_VERSION_PATCH ${HAM_VERSION_PATCH})
set(HAM_ENGINE_VERSION       ${HAM_VERSION})

set(HAM_ENGINE_BUILD_TYPE     ${HAM_BUILD_TYPE})
set(HAM_ENGINE_BUILD_TYPE_STR ${HAM_BUILD_TYPE_STR})

set(HAM_ENGINE_UUID_NAMESPACE "94d6f295-4e71-4b60-a7e5-a9fdc8bef7d2")

string(
	UUID HAM_ENGINE_CLIENT_PLUGIN_UUID
	NAMESPACE ${HAM_ENGINE_UUID_NAMESPACE}
	NAME "ham-engine-client"
	TYPE SHA1
)

string(
	UUID HAM_ENGINE_SERVER_PLUGIN_UUID
	NAMESPACE ${HAM_ENGINE_UUID_NAMESPACE}
	NAME "ham-engine-server"
	TYPE SHA1
)

configure_file(
	include/ham/engine/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/ham/engine/config.h
	@ONLY
	NEWLINE_STYLE UNIX
)

set(HAM_ENGINE_SOURCE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(HAM_ENGINE_BINARY_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)

set(
	HAM_ENGINE_SOURCE_HEADERS
	${HAM_ENGINE_BINARY_INCLUDE_DIR}/ham/engine/config.h

	${HAM_ENGINE_SOURCE_INCLUDE_DIR}/ham/engine.h
	${HAM_ENGINE_SOURCE_INCLUDE_DIR}/ham/engine-vtable.h

	${HAM_ENGINE_SOURCE_INCLUDE_DIR}/ham/engine/net.h
)

##
# Subdirs
##

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(editor)

##
# Installation
##

if(HAM_INSTALL_ENGINE)
	install(
		FILES LICENSE.md
		DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ham/libham-engine
	)

	install(
		FILES LICENSE.md
		DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ham/ham-engine-server
	)

	install(
		FILES LICENSE.md
		DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ham/ham-engine-client
	)

	install(
		FILES LICENSE.md
		DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ham/ham-engine-editor
	)

	install(
		DIRECTORY ${HAM_ENGINE_BINARY_INCLUDE_DIR}/include/ham
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		FILES_MATCHING
		    PATTERN "*.h"
			PATTERN "*.hpp"
	)

	install(
		DIRECTORY ${HAM_ENGINE_SOURCE_INCLUDE_DIR}/include/ham
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		FILES_MATCHING
			PATTERN "*.h"
			PATTERN "*.hpp"
	)

	install(
		TARGETS ham-engine ham-engine-client ham-engine-server ham-engine-editor
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
endif()