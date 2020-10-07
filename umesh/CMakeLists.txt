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

# ------------------------------------------------------------------
# main umesh/umesh library and classes
# ------------------------------------------------------------------
add_library(umesh
  # ------------------------------------------------------------------
  # the class itself, with whatever is requited to create and
  # manipulate it
  # ------------------------------------------------------------------
  
  UMesh.h
  UMesh.cpp

  RemeshHelper.h
  RemeshHelper.cpp

  TetConn.h
  TetConn.cpp
  
  # ------------------------------------------------------------------
  # I/O routines that can directly read into a umesh class
  # ------------------------------------------------------------------
  
  # specially modified fun3d file format that uses 64-bit counters and
  # indices
  io/ugrid64.cpp
  
  # "regular" fun3d files use 32-bit indices, so this is the "regular"
  # fun3d format
  io/ugrid32.cpp

  # nateive .umesh format
  io/UMesh.cpp

  # "binary-triangle-mesh" format
  io/btm/BTM.cpp

  # tetrahedralize a general umesh into a tet-mesh
  tetrahedralize.cpp
  )

target_link_libraries(umesh
  ${OWL_LIBRARIES}
  )

# ------------------------------------------------------------------
# converters, that convert from one set of files in another format to
# umesh
# ------------------------------------------------------------------
add_subdirectory(converters)





