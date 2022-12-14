set(CMAKE_SYSTEM_NAME "Windows")
#set(CMAKE_SYSTEM_PROCESSOR "x86_64")

set(CMAKE_SYSTEM_VERSION 10.0)

set(TOOLCHAIN_EXEC_PREFIX x86_64-w64-mingw32-)

set(TOOLCHAIN_CFLAGS "-pipe -march=x86-64-v3 -fopenmp -flto")
set(TOOLCHAIN_CFLAGS_DEBUG "${TOOLCHAIN_CFLAGS} -O2 -ggdb")
set(TOOLCHAIN_CFLAGS_MINSIZEREL "${TOOLCHAIN_CFLAGS} -Os -g0")
set(TOOLCHAIN_CFLAGS_RELWITHDEBINFO "${TOOLCHAIN_CFLAGS} -O3 -g3")
set(TOOLCHAIN_CFLAGS_RELEASE "${TOOLCHAIN_CFLAGS} -O3 -g0")

set(CMAKE_C_COMPILER ${TOOLCHAIN_EXEC_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_EXEC_PREFIX}g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_EXEC_PREFIX}windres)

set(CMAKE_C_FLAGS "${TOOLCHAIN_CFLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${TOOLCHAIN_CFLAGS_DEBUG}")
set(CMAKE_C_FLAGS_MINSIZEREL "${TOOLCHAIN_CFLAGS_MINSIZEREL}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${TOOLCHAIN_CFLAGS_RELWITHDEBINFO}")
set(CMAKE_C_FLAGS_RELEASE "${TOOLCHAIN_CFLAGS_RELEASE}")

set(CMAKE_CXX_FLAGS "${TOOLCHAIN_CFLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${TOOLCHAIN_CFLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${TOOLCHAIN_CFLAGS_MINSIZEREL}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${TOOLCHAIN_CFLAGS_RELWITHDEBINFO}")
set(CMAKE_CXX_FLAGS_RELEASE "${TOOLCHAIN_CFLAGS_RELEASE}")

set(CMAKE_RC_FLAGS "-DGCC_WINDRES")

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll" ".lib" ".a")
