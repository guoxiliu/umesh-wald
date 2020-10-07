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

CUDA_SELECT_NVCC_ARCH_FLAGS(ARCH_FLAGS Auto)
LIST(APPEND CUDA_NVCC_FLAGS ${ARCH_FLAGS})
message("selected cuda arch flags '${ARCH_FLAGS}'")

# ------------------------------------------------------------------
# compute connectiviy of a umesh, using GPGPU algorithm - once via
# cpu/tbb, once via gpu/cuda
# ------------------------------------------------------------------
if (CUDA_FOUND)
  cuda_add_executable(umeshComputeFacesGPU
    gpu.cu
    )
  target_link_libraries(umeshComputeFacesGPU
    umesh
  )
endif()

add_executable(umeshComputeFacesCPU
  cpu.cpp
  )
target_link_libraries(umeshComputeFacesCPU
  umesh
  )

# reference-only, using umesh/TetConn code
add_executable(umeshComputeFacesREF
  ref.cpp
  )
target_link_libraries(umeshComputeFacesREF
  umesh
  )
