cmake_minimum_required(VERSION 3.30)

project(vulkan_mandelbrot_renderer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VULKAN_SDK_REQUIRED_VERSION 1.3)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(Vulkan ${VULKAN_SDK_REQUIRED_VERSION} REQUIRED COMPONENTS glslc) 

include(FetchContent)

FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed # v1.0.3
	SYSTEM
)

FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
	SYSTEM
)

FetchContent_MakeAvailable(
	glm
	stb
)

add_executable(vulkan_mandelbrot_renderer
	src/Application.cpp
	src/Image2D.cpp
	src/input.cpp
	src/LibraryBuild.cpp
	src/Main.cpp
	src/platform/win32/window_win32.cpp
)

target_include_directories(vulkan_mandelbrot_renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})


target_link_libraries(vulkan_mandelbrot_renderer PRIVATE
	Vulkan::Vulkan
	glm::glm
	stb
)

target_compile_definitions(vulkan_mandelbrot_renderer PRIVATE
	$<$<CONFIG:Debug>:APP_DEBUG>
)

option(ENABLE_ASAN "Build with AddressSanitizer" OFF)

if(ENABLE_ASAN AND MSVC)
	target_compile_options(vulkan_mandelbrot_renderer PRIVATE /fsanitize=address)
	target_link_options(vulkan_mandelbrot_renderer PRIVATE /INCREMENTAL:NO)
endif()

if(MSVC)
	target_link_options(vulkan_mandelbrot_renderer PRIVATE 
		"/ENTRY:wWinMainCRTStartup"
	)

	set_target_properties(vulkan_mandelbrot_renderer PROPERTIES
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(vulkan_mandelbrot_renderer PRIVATE
		/clang:-Werror
		/clang:-Wall
		/clang:-Wextra
		/clang:-Wshadow
		/clang:-Wconversion
		/clang:-Wimplicit-fallthrough
		/clang:-Wabstract-vbase-init
		/clang:-Wctor-dtor-privacy
		/clang:-Wdelete-non-virtual-dtor
		/clang:-Wtautological-compare
		/clang:-Wuninitialized
		/clang:-Wloop-analysis
		/clang:-Wrange-loop-construct
		/clang:-Wthread-safety
		/clang:-Wno-c++98-compat
		/clang:-Wno-c++98-compat-pedantic
		/clang:-Wno-old-style-cast
		/clang:-Wno-newline-eof
		/clang:-Wno-error=delete-incomplete
		/clang:-Wno-braced-scalar-init
		/clang:-Wno-missing-designated-field-initializers
		/clang:-std=c++23
	)
elseif(MSVC)
	target_compile_options(vulkan_mandelbrot_renderer PRIVATE
		/WX
		/W4
		/permissive-
		/w14242
		/w14254
		/w14263
		/w14265
		/w14287
		/w14296
		/w14311
		/w14545
		/w14546
		/w14547
		/w14549
		/w14555
		/w14619
		/w14640
		/w14826
		/w14905
		/w14906
		/w14928
		/w14062
		/w14456
		/w14457
		/w14458
		/utf-8
		/Zc:__cplusplus
	)
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(SHADER_SOURCES
      ${SHADER_DIR}/vertexShader.vert
      ${SHADER_DIR}/fragmentShader.frag
      ${SHADER_DIR}/computeShader.comp
)

set(SPIRV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
      get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
      set(SPIRV ${SHADER_DIR}/${SHADER_NAME}.spv)
      add_custom_command(
              OUTPUT ${SPIRV}
              COMMAND Vulkan::glslc ${SHADER} -o ${SPIRV}
              DEPENDS ${SHADER}
              COMMENT "Compiling shader ${SHADER_NAME}"
              VERBATIM
      )
      list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_FILES})
add_dependencies(vulkan_mandelbrot_renderer shaders)
