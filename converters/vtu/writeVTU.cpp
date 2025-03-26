
/**
 * A tool to convert from umesh format to vtk vtu format
 * Author: Guoxi Liu (liuguoxi888@gmail.com)
 */

#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include "umesh/io/UMesh.h"
#include "umesh/math.h"
#include "umesh/UMesh.h"

int main ( int argc, char *argv[] )
{
  std::string outFileName = "";
  std::string inFileName = "";
  
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg == "-o")
      outFileName = argv[++i];
    else
      inFileName = arg;
  }
  
  if(inFileName.empty() || outFileName.empty())
  {
    std::cerr << "Usage: " << argv[0] << " -o outfile.vtu <infile.umesh>" << std::endl;
    return EXIT_FAILURE;
  }

  // read umesh file
  std::cout << "parsing umesh file " << inFileName << std::endl;
  umesh::UMesh::SP inMesh = umesh::io::loadBinaryUMesh(inFileName);
  std::cout << "Done reading.\n UMesh info:\n" << inMesh->toString(false) << std::endl;

  // create a new VTK unstructured grid
  vtkNew<vtkUnstructuredGrid> grid;

  // insert points
  vtkNew<vtkPoints> points;
  for (umesh::vec3f &p : inMesh->vertices) {
    points->InsertNextPoint(p.x, p.y, p.z);
  }
  grid->SetPoints(points);

  std::cout << "Successfully insert all the points!" << std::endl;

  // insert per-vertex data 
  if (inMesh->perVertex == nullptr) {
    std::cout << "The input umesh does not contain per-vertex data!" << std::endl;
  } else {
    std::cout << "per-vertex size: " << inMesh->perVertex->values.size() << std::endl;
    vtkNew<vtkFloatArray> arr;
    arr->SetNumberOfComponents(1);
    arr->SetComponentName(0, inMesh->perVertex->name.c_str());
    arr->SetArray(&inMesh->perVertex->values.operator[](0), inMesh->perVertex->values.size(), 1);
    std::cout << "vtk scalar field size: " << arr->GetNumberOfTuples() << std::endl;
    grid->GetPointData()->AddArray(arr);
    std::cout << "Successfully insert the scalar value for the vertex!" << std::endl;
  }

  // insert triangles
  if (!inMesh->triangles.empty()) {
    for (auto &tri : inMesh->triangles) {
      vtkIdType triangle[] = {tri[0], tri[1], tri[2]};
      grid->InsertNextCell(VTK_TRIANGLE, 3, triangle);
    }
    std::cout << "Successfully insert all the triangles!" << std::endl;
  }

  // insert quadrangles
  if (!inMesh->quads.empty()) {
    for (auto &quad : inMesh->quads) {
      vtkIdType quadrangle[] = {quad[0], quad[1], quad[2], quad[3]};
      grid->InsertNextCell(VTK_QUAD, 4, quadrangle);
    }
    std::cout << "Successfully insert all the quadrangles!" << std::endl;
  }

  // insert tetrahedra
  if (!inMesh->tets.empty()) {
    for (auto &tet : inMesh->tets) {
      vtkIdType tetrahedron[] = {tet[0], tet[1], tet[2], tet[3]};
      grid->InsertNextCell(VTK_TETRA, 4, tetrahedron);
    }
    std::cout << "Successfully insert all the tetrahedra!" << std::endl;
  }

  // insert pyramids
  if (!inMesh->pyrs.empty()) {
    for (auto &pyr : inMesh->pyrs) {
      vtkIdType pyramid[] = {pyr[0], pyr[1], pyr[2], pyr[3], pyr[4]};
      grid->InsertNextCell(VTK_PYRAMID, 5, pyramid);
    }
    std::cout << "Successfully insert all the pyramids!" << std::endl;
  }

  // insert wedges
  if (!inMesh->wedges.empty()) {
    for (auto &we : inMesh->wedges) {
      vtkIdType wedge[] = {we[0], we[1], we[2], we[3], we[4], we[5]};
      grid->InsertNextCell(VTK_WEDGE, 6, wedge);
    }
    std::cout << "Successfully insert all the wedges!" << std::endl;
  }

  // insert hexahedra
  if (!inMesh->hexes.empty()) {
    for (auto &hex : inMesh->hexes) {
      vtkIdType hexahedron[] = {hex[0], hex[1], hex[2], hex[3], hex[4], hex[5], hex[6], hex[7]};
      grid->InsertNextCell(VTK_HEXAHEDRON, 8, hexahedron);
    }
    std::cout << "Successfully insert all the hexahedra!" << std::endl;
  }

  std::cout << "=======================================================" << std::endl;
  std::cout << "writing out result ..." << std::endl;
  std::cout << "=======================================================" << std::endl;

  vtkNew<vtkXMLUnstructuredGridWriter> writer;
  writer->SetFileName(outFileName.c_str());
  writer->SetInputData(grid);
  writer->Write();
  
  return EXIT_SUCCESS;
}
