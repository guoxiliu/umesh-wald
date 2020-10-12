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
project(UMesh VERSION 1.2.0)

if (NOT (${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_HOME_DIRECTORY}))
  SET(UMESH_IS_SUBPROJECT ON)
else()
  SET(UMESH_IS_SUBPROJECT OFF)
endif()

cmake_minimum_required(VERSION 2.8)

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




# ------------------------------------------------------------------
# include owl, for owl::parallel_for and owl::vec3f
# -----------------------------------------------------------------

set(owl_dir ${PROJECT_SOURCE_DIR}/submodules/owl)
add_subdirectory(${owl_dir} external_owl EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------
# common settings across entire project
# -----------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${OWL_INCLUDES})
add_definitions(${OWL_CXX_FLAGS})


# ------------------------------------------------------------------
# everything relating to umesh format/data/library
# -----------------------------------------------------------------
add_subdirectory(umesh)

# ------------------------------------------------------------------
# cmdline-tools
# -----------------------------------------------------------------
add_subdirectory(apps)


# export owl variables back up to parent, in case parent needs to use OWL
if (UMESH_IS_SUBPROJECT)
  set(UMESH_LIBRARIES umesh ${OWL_LIBRARIES} PARENT_SCOPE)
  set(UMESH_INCLUDES ${PROJECT_SOURCE_DIR} ${OWL_INCLUDES} PARENT_SCOPE)
  set(UMESH_CXX_FLAGS ${OWL_CXX_FLAGS} PARENT_SCOPE)
  set(OWL_VIEWER_LIBRARIES ${OWL_VIEWER_LIBRARIES} PARENT_SCOPE)
endif()
