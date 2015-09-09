include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SRC_FILES bonez/*.cpp bonez/*.hpp bonez/*.h bonez/*.hxx bonez/*.inl bonez/*.vs bonez/*.fs bonez/*.gs bonez/*.cs bonez/*.xs)

GROUP_SOURCES(${CMAKE_CURRENT_SOURCE_DIR}/bonez bonez)

add_library(bonez STATIC ${SRC_FILES})

set_target_properties(bonez PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${CMAKE_CURRENT_SOURCE_DIR}/bonez/stdafx.h)
cotire(bonez)

BNZ_ADD_SHADER_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/bonez/opengl/shaders)
BNZ_ADD_SHADER_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/bonez/voxskel/shaders)
