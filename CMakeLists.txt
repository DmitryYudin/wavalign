cmake_minimum_required(VERSION 3.14)

project(wavalign)

# prefer minimal size by default
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_MINSIZEREL}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_MINSIZEREL}")

if(CMAKE_CONFIGURATION_TYPES) # https://stackoverflow.com/questions/31661264/cmake-generators-for-visual-studio-do-not-set-cmake-configuration-types
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Debug/Release only" FORCE)
endif()
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
source_group(src REGULAR_EXPRESSION ".*\\.[ch].*")

add_library(libkissfft STATIC)
set(libkissfft_SRC
	kissfft/kiss_fft.c
	kissfft/kiss_fftr.c
)
target_sources(libkissfft PRIVATE
	${libkissfft_SRC}
)
target_compile_definitions(libkissfft PUBLIC kiss_fft_scalar=float)
target_include_directories(libkissfft PUBLIC kissfft)

add_library(libwavalign STATIC)
set(libwavalign_SRC
	src/xcorr.cc
	src/ssd.cc
	src/bestoffset.cc
)
target_sources(libwavalign PRIVATE
	${libwavalign_SRC}
)
target_include_directories(libwavalign PUBLIC src)
target_link_libraries(libwavalign libkissfft)

#
# Tests
#
if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
	add_library(libwavfile STATIC)
	set(libwavfile_SRC
		test/wavfile/wavreader.c
		test/wavfile/wavwriter.c
	)
	target_sources(libwavfile PRIVATE
		${libwavfile_SRC}
	)
	target_include_directories(libwavfile PUBLIC test/wavfile)

    set(wavalign_SRC
		test/wavalign.cc
	)
    set(unittest_wavfmt_SRC test/wavfile/unittest_wavfmt.cc)
	foreach(X IN ITEMS
		wavalign
		unittest_wavfmt
	)
	    add_executable(${X})
	    target_sources(${X} PRIVATE ${${X}_SRC})
	    target_link_libraries(${X} libwavfile)
	endforeach()
    target_link_libraries(wavalign libwavalign)
	target_include_directories(wavalign PRIVATE test/win32)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT wavalign)
endif()
