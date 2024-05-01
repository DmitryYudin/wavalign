cmake_minimum_required(VERSION 3.14)

project(wavalign)

source_group(src REGULAR_EXPRESSION ".*\\.[ch].*")

# prefer minimal size by default
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_MINSIZEREL}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_MINSIZEREL}")

if(CMAKE_CONFIGURATION_TYPES) # https://stackoverflow.com/questions/31661264/cmake-generators-for-visual-studio-do-not-set-cmake-configuration-types
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Debug/Release only" FORCE)
endif()
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_library(libkissfft STATIC)
file(GLOB_RECURSE libkissfft_SRC "kissfft/*.[ch]*")
target_sources(libkissfft PRIVATE ${libkissfft_SRC})
target_compile_definitions(libkissfft PUBLIC kiss_fft_scalar=float)
target_include_directories(libkissfft INTERFACE kissfft)

add_library(libwavalign STATIC)
file(GLOB_RECURSE libwavalign_SRC "src/*.[ch]*")
target_sources(libwavalign PRIVATE ${libwavalign_SRC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${libwavalign_SRC})
target_include_directories(libwavalign INTERFACE src)
target_link_libraries(libwavalign libkissfft)

# Tests
if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
	file(GLOB_RECURSE libwavfile_SRC "test/wavfile/*.h*" test/wavfile/wavreader.c test/wavfile/wavwriter.c)
    set(wavalign_SRC test/wavalign.cc)
    set(unittest_wavfmt_SRC test/wavfile/unittest_wavfmt.cc)
	foreach(X IN ITEMS
		wavalign
		unittest_wavfmt
	)
	    add_executable(${X})
	    target_sources(${X} PRIVATE ${${X}_SRC} ${libwavfile_SRC})
		target_include_directories(${X} PRIVATE test/wavfile)
	endforeach()
    target_link_libraries(wavalign libwavalign)
    if(WIN32)
    	target_include_directories(wavalign PRIVATE test/win32)
    endif()
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT wavalign)
endif()
