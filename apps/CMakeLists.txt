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

add_definitions(${OWL_CXX_FLAGS})
find_package(CUDA)

# ------------------------------------------------------------------
# print basic info of a umesh: #verts, tris, quads, tets, etc, as well
# as bounding box.
# ------------------------------------------------------------------
add_executable(umeshInfo
  info.cpp
  )
target_link_libraries(umeshInfo
  umesh
  )

# ------------------------------------------------------------------
# tetrahedralize a (general) umesh into a tet-only umesh, by
# introducing new vertices into quad centers
# ------------------------------------------------------------------
add_executable(umeshTetrahedralize
  tetrahedralize.cpp
  )
target_link_libraries(umeshTetrahedralize
  umesh
  )


# ------------------------------------------------------------------
# computes the connectivity (tet and facets per face, and faces per tet) for a given tet mesh. only allowed for tet meshes
# ------------------------------------------------------------------
add_executable(umeshComputeTetConnectivity
  computeTetConnectivity.cpp
  )
target_link_libraries(umeshComputeTetConnectivity
  umesh
  )


# ------------------------------------------------------------------
# computes the outer shell of a tet-mesh, ie, all the triangle and/or
# bilinear faces that are _not_ shared between two neighboring
# elements
# ------------------------------------------------------------------
add_executable(umeshComputeShell
  computeShell.cpp
  )
target_link_libraries(umeshComputeShell
  umesh
  )

# ------------------------------------------------------------------
# partitions a umesh, into either a BVH, or kd-tree. Note we don't
# actually save teh kdtree/bvh, only the final bricks, and the
# primrefs that refer into the original mesh.
# ------------------------------------------------------------------
add_executable(umeshPartitionSpatially
  partitionSpatially.cpp
  )
target_link_libraries(umeshPartitionSpatially
  umesh
  )

add_executable(umeshPartitionObjectSpace
  partitionObjectSpace.cpp
  )
target_link_libraries(umeshPartitionObjectSpace
  umesh
  )



# ------------------------------------------------------------------
# computes faces, and face-connectiviy information, for a umesh; once
# via gpu, once via tbb
# ------------------------------------------------------------------
add_subdirectory(computeFaces)
