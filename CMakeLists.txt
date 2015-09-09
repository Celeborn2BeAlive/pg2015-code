cmake_minimum_required(VERSION 2.8)

project(pg2015-code)

# Set the build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message("Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable C++11
include(CXX11)
check_for_cxx11_compiler(CXX11_COMPILER)

if(CXX11_COMPILER)
    enable_cxx11()
endif()

# Include cotire for precompiled headers
include(cotire)

# Change output directories for libraries and binaries
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# A macro in order to set shaders as target for the build solution
macro(BNZ_ADD_SHADER_DIRECTORY directory)
    file(GLOB_RECURSE relative_files RELATIVE ${directory} ${directory}/*.vs ${directory}/*.fs ${directory}/*.cs ${directory}/*.gs ${directory}/*.xs)
    file(GLOB_RECURSE files ${directory}/*.vs ${directory}/*.fs ${directory}/*.cs ${directory}/*.gs ${directory}/*.xs)

    if(files)
        list(LENGTH files file_count)
        math(EXPR range_end "${file_count} - 1")

        foreach(idx RANGE ${range_end})
            list(GET files ${idx} file)
            list(GET relative_files ${idx} relative_file)

            if(MSVC)
            	set(SHADER_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH}//\$\(Configuration\)/shaders/${relative_file})
            else()
            	set(SHADER_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH}/shaders/${relative_file})
            endif()

            add_custom_command(OUTPUT ${EXECUTABLE_OUTPUT_PATH}/shaders/${relative_file}
                             COMMAND ${CMAKE_COMMAND} -E copy ${file} ${SHADER_OUTPUT_PATH}
                             MAIN_DEPENDENCY ${file})
        endforeach()
    endif()
endmacro()

# A macro to get all subdirectories of a given directory
macro(SUBDIRLIST result curdir)
	file(GLOB children RELATIVE ${curdir} ${curdir}/*)
	set(dirlist "")
	foreach(child ${children})
	if(IS_DIRECTORY ${curdir}/${child})
		list(APPEND dirlist ${child})
	endif()
	endforeach()
	set(${result} ${dirlist})
endmacro()

macro(GROUP_SOURCES curdir groupname)
	file(GLOB children RELATIVE ${curdir} ${curdir}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${curdir}/${child})
			GROUP_SOURCES(${curdir}/${child} ${groupname}/${child})
		else()
			string(REPLACE "/" "\\" groupname2 ${groupname})
			source_group(${groupname2} FILES ${curdir}/${child})
		endif()
	endforeach()
endmacro()

macro(ADD_BONEZ_EXECUTABLE name src_files)
    GROUP_SOURCES(${CMAKE_CURRENT_SOURCE_DIR}/src ${name})

    add_executable(${name} ${src_files})

    target_link_libraries(${name}
        ${BONEZ_LIBRARY}
        ${GLFW_LIBRARY}
        ${EMBREE_LIBRARY}
        ${ASSIMP_LIBRARY}
        ${OPENGL_LIBRARIES}
        ${GLEW_LIBRARY}
        ${SYSTEM_LIBRARIES}
        ${OPENEXR_LIBRARIES})

    set_target_properties(${name} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.h)
    cotire(${name})

    if(MSVC)
            foreach(DLL_FILE ${DLL_FILES})
                    add_custom_command( TARGET ${EXECUTABLE_FILE} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy ${DLL_FILE} ${EXECUTABLE_OUTPUT_PATH}/\$\(Configuration\)
                            COMMENT "copying dll ${DLL_FILE}"
                    )
            endforeach()
    endif()
endmacro()

# Find libraries
find_package(OpenGL REQUIRED)

if(WIN32)
    if(MSVC)
        set(LIBRARY_DIRECTORIES ${CMAKE_SOURCE_DIR}/third-party/lib/windows-x64/)
        set(GLEW_LIBRARY glew32s.lib)
        set(GLFW_LIBRARY glfw3dll.lib)
        set(EMBREE_LIBRARY embree.lib)
        set(ASSIMP_LIBRARY assimp.lib)
        set(BONEZ_LIBRARY bonez)
        set(SYSTEM_LIBRARIES Shlwapi)
        set(OPENEXR_LIBRARIES IlmImf-2_2.lib IlmImfUtil-2_2.lib Imath-2_2.lib IexMath-2_2.lib Half.lib Iex-2_2.lib IlmThread-2_2.lib)

        add_definitions(-DGLEW_NO_GLU -DGLEW_STATIC -DNOMINMAX)

		set(DLL_3RD ${CMAKE_SOURCE_DIR}/third-party/lib/windows-x64)
		file(GLOB DLL_FILES ${DLL_3RD}/*.dll)
	else()
		message(FATAL_ERROR "Windows only supported for MSVC builds")
    endif()
elseif(UNIX)
    if(APPLE)
        message(FATAL_ERROR "Apple platform unsupported")
    else()
        set(LIBRARY_DIRECTORIES ${CMAKE_SOURCE_DIR}/third-party/lib/linux-x64/)
        set(GLEW_LIBRARY GLEW)
        set(GLFW_LIBRARY glfw)
        set(EMBREE_LIBRARY embree)
        set(ASSIMP_LIBRARY assimp)
        set(BONEZ_LIBRARY bonez)
        set(OPENEXR_LIBRARIES libIlmImf-2_2.so libIlmImfUtil-2_2.so Imath-2_2 IexMath-2_2 Half Iex-2_2 IlmThread-2_2 pthread)

        set(CMAKE_CXX_COMPILER "g++")
        set(CMAKE_C_COMPILER "gcc")
        set(CMAKE_CXX_FLAGS "-Wall -fPIC -fvisibility-inlines-hidden -fvisibility=hidden ${SSE_FLAGS} -std=c++14 -fno-reciprocal-math")
        set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g -gdwarf-2 -ftree-ter")
        set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -g -gdwarf-2 -O2")
        set(CMAKE_EXE_LINKER_FLAGS "")
    endif()
endif()

link_directories(${LIBRARY_DIRECTORIES})
include_directories(${CMAKE_SOURCE_DIR}/third-party/include ${CMAKE_SOURCE_DIR}/third-party/include/OpenEXR)

set(SSE_VERSION "SSE4.2" CACHE INT "SSE version to use (SSSE3,SSE4.1,SSE4.2,AVX)")
set(CMAKE_VERBOSE_MAKEFILE false)

if(SSE_VERSION STREQUAL "SSSE3")
    set(SSE_FLAGS "-mssse3")
elseif(SSE_VERSION STREQUAL "SSE4.1")
    set(SSE_FLAGS "-msse4.1")
elseif(SSE_VERSION STREQUAL "SSE4.2")
    set(SSE_FLAGS "-msse4.2")
elseif(SSE_VERSION STREQUAL "AVX")
    set(SSE_FLAGS "-mavx")
else()
    message(FATAL_ERROR "SSE Version ${SSE_VERSION} not supported use SSSE3, SSE4.1, SSE4.2 or AVX" )
endif()

include_directories(${CMAKE_SOURCE_DIR}/bonez/)

add_subdirectory(bonez)
add_subdirectory(apps)
