
cmake_minimum_required(VERSION 3.3 FATAL_ERROR)

project(convertVTU)

find_package(VTK COMPONENTS 
  vtkCommonColor
  vtkCommonCore
  vtkIOXML
  vtkInteractionStyle
  vtkRenderingCore
  vtkRenderingFreeType
  vtkRenderingOpenGL2 QUIET)
if (NOT VTK_FOUND)
  message("VTK Not Found. Skipping convertVTU.")
  return ()
endif()
message (STATUS "VTK_VERSION: ${VTK_VERSION}")
if (VTK_VERSION VERSION_LESS "8.90.0")
  # old system
  include(${VTK_USE_FILE})
  add_executable(readVTU MACOSX_BUNDLE readVTU.cpp )
  target_link_libraries(readVTU PRIVATE ${VTK_LIBRARIES})
  add_executable(writeVTU MACOSX_BUNDLE writeVTU.cpp )
  target_link_libraries(writeVTU PRIVATE ${VTK_LIBRARIES} umesh)
else ()
  # include all components
  add_executable(readVTU MACOSX_BUNDLE readVTU.cpp )
  target_link_libraries(readVTU PRIVATE ${VTK_LIBRARIES})
  add_executable(writeVTU MACOSX_BUNDLE writeVTU.cpp )
  target_link_libraries(writeVTU PRIVATE ${VTK_LIBRARIES} umesh)
  # vtk_module_autoinit is needed
  vtk_module_autoinit(
    TARGETS readVTU
    MODULES ${VTK_LIBRARIES}
    )
endif () 
