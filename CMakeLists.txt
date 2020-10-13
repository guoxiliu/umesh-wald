# ======================================================================== #
# Copyright 2018-2020 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
project(UMesh VERSION 1.3.0)

if (NOT (${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_HOME_DIRECTORY}))
  SET(UMESH_IS_SUBPROJECT ON)
else()
  SET(UMESH_IS_SUBPROJECT OFF)
endif()

cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}")

if(NOT SET_UP_CONFIGURATIONS_DONE)
  set(SET_UP_CONFIGURATIONS_DONE 1)

  # No reason to set CMAKE_CONFIGURATION_TYPES if it's not a multiconfig generator
  # Also no reason mess with CMAKE_BUILD_TYPE if it's a multiconfig generator.
  if(CMAKE_CONFIGURATION_TYPES) # multiconfig generator?
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE) 
  else()
    if(NOT CMAKE_BUILD_TYPE)
      #            message("Defaulting to release build.")
      set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose the type of build")
    # set the valid options for cmake-gui drop-down list
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release")
  endif()
  
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

set(UMESH_INCLUDES ${PROJECT_SOURCE_DIR})

if (WIN32)
  OPTION(UMESH_USE_TBB "Use TBB to parallelize host-side code?" OFF)
else()
  OPTION(UMESH_USE_TBB "Use TBB to parallelize host-side code?" ON)
endif()

if (UMESH_USE_TBB)
  find_package(TBB)
  if (TBB_FOUND)
    #    include_directories(${TBB_INCLUDE_DIR})
    set(UMESH_CXX_FLAGS "${UMESH_CXX_FLAGS} -DUMESH_HAVE_TBB=1")
    set(UMESH_HAVE_TBB ON)
    
    set(UMESH_INCLUDES ${UMESH_INCLUDES} ${TBB_INCLUDE_DIR})
    set(UMESH_LIBRARIES ${UMESH_LIBRARIES} ${TBB_LIBRARIES})
    set(UMESH_DEFINITIONS -DUMESH_HAVE_TBB=1)
  else()
    set(UMESH_HAVE_TBB OFF)
    message("#umesh.cmake: TBB not found; falling back to serial execution of umesh::parallel_for")
  endif()
endif()


# ------------------------------------------------------------------
# common settings across entire project
# -----------------------------------------------------------------
include_directories(${UMESH_INCLUDES})
add_definitions(${UMESH_DEFINITIONS})

# ------------------------------------------------------------------
# everything relating to umesh format/data/library
# -----------------------------------------------------------------
add_subdirectory(umesh)

# ------------------------------------------------------------------
# cmdline-tools
# -----------------------------------------------------------------
add_subdirectory(apps)

# export our make variables back up to parent, so parent can do things
# like "include_directories(${UMESH_INCLUDES})" etc
if (UMESH_IS_SUBPROJECT)
  set(UMESH_LIBRARIES ${UMESH_LIBRARIES} umesh PARENT_SCOPE)
  set(UMESH_INCLUDES ${UMESH_INCLUDES} PARENT_SCOPE)
  set(UMESH_DEFINITIONS ${UMESH_DEFINITIONS} PARENT_SCOPE)
endif()
