cmake_minimum_required (VERSION 3.5.1)
project (XPLLOCK)
set(CMAKE_BUILD_TYPE Debug)

include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}/include		# To find xpllock.h
)

include(cmake/deps.cmake)
include(cmake/platform.cmake)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/xpllock.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/xpllock.h)

set(HAVE_SEMAPHORE_H 1)

include_directories(include)


add_subdirectory(src)

install(
	DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/include/
	DESTINATION	/usr/include

	PATTERN		"*"			PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ
)
