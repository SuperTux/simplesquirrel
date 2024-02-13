cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(simplesquirrel)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# Some required properties
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Select build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_INSTALL "Install library" ON)

option(USE_SQ_SUBMODULE "Use the squirrel submodule as opposed to the system squirrel" ON)

# Add third party libraries
if(USE_SQ_SUBMODULE)
  add_subdirectory("libs/squirrel")
else()
  find_library(SQUIRREL names squirrel)
  find_library(SQUIRREL_STDLIB names sqstdlib)
endif()

# Grab the files
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/simplesquirrel/*.hpp)

include_directories(${INCLUDE_DIRECTORIES} ${SQUIRREL_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${CMAKE_LIBRARY_PATH})

# Add our library
add_library(${PROJECT_NAME}_static STATIC ${SOURCES} ${HEADERS})
add_library(${PROJECT_NAME}-static ALIAS ${PROJECT_NAME}_static)
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

if(USE_SQ_SUBMODULE)
  include_directories(${PROJECT_SOURCE_DIR}/libs/squirrel/include)
  target_link_libraries(${PROJECT_NAME} squirrel_static sqstdlib_static)
else()
  target_link_libraries(${PROJECT_NAME} squirrel sqstdlib)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE SSQ_EXPORTS=1 SSQ_DLL=1)

set_target_properties(${PROJECT_NAME}_static PROPERTIES
  FOLDER "simplesquirrel/lib"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
  FOLDER "simplesquirrel/lib"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Install
if(BUILD_INSTALL)
  if(MSVC)
      INSTALL(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION bin OPTIONAL)
  endif()
  INSTALL(TARGETS ${PROJECT_NAME}_static ARCHIVE DESTINATION lib LIBRARY DESTINATION lib COMPONENT library)
  INSTALL(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin COMPONENT library)
  INSTALL(DIRECTORY include/simplesquirrel DESTINATION include)
endif()

# Build Tests
if(BUILD_TESTS)
    # Enable testing (done by catch.hpp)
    enable_testing()
    add_subdirectory(tests)
endif()

# Build Examples 
if(BUILD_TESTS)
    add_subdirectory(examples)
endif()
