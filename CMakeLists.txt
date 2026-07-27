cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR
		"In-source builds are disabled. Configure a dedicated build directory, "
		"for example: cmake -S . -B build/local")
endif()

project(subsync VERSION 0.18.0 LANGUAGES CXX)

option(SUBSYNC_ENABLE_CUDA "Build the CUDA word matching backend" OFF)
option(SUBSYNC_ENABLE_OPENCL "Build the OpenCL word matching backend" OFF)
option(SUBSYNC_BUILD_APP "Build the Python extension and media pipeline" ON)
option(SUBSYNC_BUILD_MATCHER_BENCHMARK
	"Build the standalone word matcher benchmark" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT SUBSYNC_BUILD_APP AND NOT SUBSYNC_BUILD_MATCHER_BENCHMARK)
	message(FATAL_ERROR
		"Enable SUBSYNC_BUILD_APP or SUBSYNC_BUILD_MATCHER_BENCHMARK")
endif()

set(SUBSYNC_MATCHER_SOURCES gizmo/match/wordmatcher.cpp)

if(SUBSYNC_ENABLE_CUDA)
	enable_language(CUDA)
	find_package(CUDAToolkit REQUIRED)
	list(APPEND SUBSYNC_MATCHER_SOURCES gizmo/match/wordmatcher_cuda.cu)
endif()

if(SUBSYNC_ENABLE_OPENCL)
	find_package(OpenCL REQUIRED)
	list(APPEND SUBSYNC_MATCHER_SOURCES gizmo/match/wordmatcher_opencl.cpp)
endif()

function(subsync_configure_matcher_target target)
	target_include_directories(${target} PRIVATE gizmo)

	if(SUBSYNC_ENABLE_CUDA)
		target_compile_definitions(${target} PRIVATE SUBSYNC_ENABLE_CUDA=1)
		target_link_libraries(${target} PRIVATE CUDA::cudart)
		set_target_properties(${target} PROPERTIES
			CUDA_STANDARD 17
			CUDA_STANDARD_REQUIRED ON
			CUDA_SEPARABLE_COMPILATION ON)
	endif()

	if(SUBSYNC_ENABLE_OPENCL)
		target_compile_definitions(${target} PRIVATE SUBSYNC_ENABLE_OPENCL=1)
		target_link_libraries(${target} PRIVATE OpenCL::OpenCL)
	endif()

	if(MSVC)
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:CXX>:/EHsc>
			$<$<COMPILE_LANGUAGE:CXX>:/utf-8>
			$<$<COMPILE_LANGUAGE:CXX>:/O2>)
	else()
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:CXX>:-O3>
			$<$<COMPILE_LANGUAGE:CXX>:-fvisibility=hidden>
			$<$<COMPILE_LANGUAGE:CXX>:-fomit-frame-pointer>
			$<$<COMPILE_LANGUAGE:CUDA>:-O3>)
	endif()
endfunction()

if(SUBSYNC_BUILD_APP)
	find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
	find_package(pybind11 3.0 CONFIG REQUIRED)
	find_package(PkgConfig REQUIRED)

	# FFmpeg 6.1 remains security-maintained and is the newest branch compatible
	# with the decoder interfaces used here. FFmpeg 7/8 migration is tracked
	# separately because its channel-layout and related media APIs require changes.
	pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
		"libavdevice>=60" "libavdevice<61"
		"libavformat>=60" "libavformat<61"
		"libavfilter>=9" "libavfilter<10"
		"libavcodec>=60" "libavcodec<61"
		"libswresample>=4" "libswresample<5"
		"libswscale>=7" "libswscale<8"
		"libavutil>=58" "libavutil<59")
	pkg_check_modules(POCKETSPHINX REQUIRED IMPORTED_TARGET pocketsphinx>=5.1)

	set(SUBSYNC_SOURCES
		gizmo/extractor.cpp
		gizmo/correlator.cpp
		gizmo/synchro/synchronizer.cpp
		gizmo/media/demux.cpp
		gizmo/media/stream.cpp
		gizmo/media/audiodec.cpp
		gizmo/media/resampler.cpp
		gizmo/media/subdec.cpp
		gizmo/media/speechrec.cpp
		gizmo/text/translator.cpp
		gizmo/text/dictionary.cpp
		gizmo/text/words.cpp
		gizmo/text/ssa.cpp
		gizmo/text/ngrams.cpp
		gizmo/text/utf8.cpp
		gizmo/text/wordsqueue.cpp
		gizmo/math/point.cpp
		gizmo/math/line.cpp
		gizmo/math/linefinder.cpp
		gizmo/general/exception.cpp
		gizmo/general/logger.cpp
		gizmo/general/platform.cpp
		gizmo/python/wrapper.cpp
		gizmo/python/general.cpp
		gizmo/python/extractor.cpp
		gizmo/python/media.cpp
		gizmo/python/stream.cpp
		gizmo/python/correlator.cpp
		gizmo/python/translator.cpp
		${SUBSYNC_MATCHER_SOURCES})

	pybind11_add_module(gizmo MODULE ${SUBSYNC_SOURCES})
	subsync_configure_matcher_target(gizmo)
	target_compile_definitions(gizmo PRIVATE NDEBUG=1 USE_PYBIND11=1)
	target_link_libraries(gizmo PRIVATE
		PkgConfig::FFMPEG
		PkgConfig::POCKETSPHINX)

	install(TARGETS gizmo LIBRARY DESTINATION . RUNTIME DESTINATION .)
endif()

if(SUBSYNC_BUILD_MATCHER_BENCHMARK)
	add_executable(subsync-matcher-benchmark
		gizmo/benchmark/matcher.cpp
		gizmo/text/dictionary.cpp
		gizmo/text/utf8.cpp
		gizmo/text/words.cpp
		${SUBSYNC_MATCHER_SOURCES})
	subsync_configure_matcher_target(subsync-matcher-benchmark)
endif()
