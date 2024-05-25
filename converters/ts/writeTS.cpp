
/**
 * A tool to convert from umesh format to vtk vtu format
 * Author: Guoxi Liu (liuguoxi888@gmail.com)
 */

#include "umesh/io/UMesh.h"
#include "umesh/math.h"
#include "umesh/UMesh.h"

std::vector<double> vertex;
std::vector<double> perCellValue;
std::vector<size_t> hex_index;

#ifndef PRINT
# define PRINT(var) std::cout << #var << "=" << var << std::endl;
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl;
#endif


void readFile(const std::string fileName)
{
  std::cout << "parsing umesh file " << fileName << std::endl;
  umesh::UMesh::SP inMesh = umesh::io::loadBinaryUMesh(fileName);
  std::cout << "Done reading.\n UMesh info:\n" << inMesh->toString(false) << std::endl;
}


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
    std::cerr << "Usage: " << argv[0] << " -o outfile.ts <infile.umesh>" << std::endl;
    return EXIT_FAILURE;
  }

  // read umesh file
  std::cout << "parsing umesh file " << inFileName << std::endl;
  umesh::UMesh::SP inMesh = umesh::io::loadBinaryUMesh(inFileName);
  std::cout << "Done reading.\n UMesh info:\n" << inMesh->toString(false) << std::endl;

  // check triangles
  if (!inMesh->triangles.empty()) {
    // for (auto &tri : inMesh->triangles) {
    //   vtkIdType triangle[] = {tri[0], tri[1], tri[2]};
    //   grid->InsertNextCell(VTK_TRIANGLE, 3, triangle);
    // }
    // std::cout << "Successfully insert all the triangles!" << std::endl;
    std::cerr << "Only tetrahedra are allowed in .ts file, triangles detected!" << std::endl;
    return EXIT_FAILURE;
  }

  // check quadrangles
  if (!inMesh->quads.empty()) {
    // for (auto &quad : inMesh->quads) {
    //   vtkIdType quadrangle[] = {quad[0], quad[1], quad[2], quad[3]};
    //   grid->InsertNextCell(VTK_QUAD, 4, quadrangle);
    // }
    // std::cout << "Successfully insert all the quadrangles!" << std::endl;
    std::cerr << "Only tetrahedra are allowed in .ts file, quadrangles detected!" << std::endl;
    return EXIT_FAILURE;
  }

  // check pyramids
  if (!inMesh->pyrs.empty()) {
    // for (auto &pyr : inMesh->pyrs) {
    //   vtkIdType pyramid[] = {pyr[0], pyr[1], pyr[2], pyr[3], pyr[4]};
    //   grid->InsertNextCell(VTK_PYRAMID, 5, pyramid);
    // }
    // std::cout << "Successfully insert all the pyramids!" << std::endl;
    std::cerr << "Only tetrahedra are allowed in .ts file, pyramids detected!" << std::endl;
    return EXIT_FAILURE;
  }

  // check wedges
  if (!inMesh->wedges.empty()) {
    // for (auto &we : inMesh->wedges) {
    //   vtkIdType wedge[] = {we[0], we[1], we[2], we[3], we[4], we[5]};
    //   grid->InsertNextCell(VTK_WEDGE, 6, wedge);
    // }
    // std::cout << "Successfully insert all the wedges!" << std::endl;
    std::cerr << "Only tetrahedra are allowed in .ts file, wedges detected!" << std::endl;
    return EXIT_FAILURE;
  }

  // check hexahedra
  if (!inMesh->hexes.empty()) {
    // for (auto &hex : inMesh->hexes) {
    //   vtkIdType hexahedron[] = {hex[0], hex[1], hex[2], hex[3], hex[4], hex[5], hex[6], hex[7]};
    //   grid->InsertNextCell(VTK_HEXAHEDRON, 8, hexahedron);
    // }
    // std::cout << "Successfully insert all the hexahedra!" << std::endl;
    std::cerr << "Only tetrahedra are allowed in .ts file, hexahedra detected!" << std::endl;
    return EXIT_FAILURE;
  }

  // done checking, now write the file
  std::cout << "=======================================================" << std::endl;
  std::cout << "writing out result ..." << std::endl;
  std::cout << "=======================================================" << std::endl;

  // create the output file
  FILE *file = fopen(outFileName.c_str(), "w");

  // insert points
  size_t n = inMesh->vertices.size(), m = inMesh->tets.size();
  fprintf(file, "%d %d\n", n, m);
  int barWidth = 60;
  float progress = 0.0f;

  // insert per-vertex data 
  if (inMesh->perVertex == nullptr) {
    std::cout << "The input umesh does not contain per-vertex data!" << std::endl;
    // insert vertex and its scalar value
    for (size_t i = 0; i < n; ++i) {
      umesh::vec3f &p = inMesh->vertices[i];
      fprintf(file, "%f %f %f\n", p.x, p.y, p.z);
      // print out the progress bar
      float cur = float(i + 1) / n;
      if (cur - progress >= 0.08f || i == n-1) {
        int pos = barWidth * cur;
        std::cout << "[";
        for (int j = 0; j < barWidth; ++j) {
          if (j < pos) std::cout << "=";
          else if (j == pos) std::cout << ">";
          else std::cout << " ";
        }
        std::cout << "] " << int(cur * 100.0) << "% \r";
        std::cout.flush();
        progress = cur;
      }
    }
    std::cout << "\nSuccessfully insert all the points!" << std::endl;
  } else {
    if (inMesh->perVertex->values.size() != inMesh->vertices.size()) {
      std::cerr << "The size of scalars does not matach the number of vertices!" << std::endl;
      return EXIT_FAILURE;
    }
    // insert vertex and its scalar value
    for (size_t i = 0; i < n; ++i) {
      umesh::vec3f &p = inMesh->vertices[i];
      fprintf(file, "%f %f %f %f\n", p.x, p.y, p.z, inMesh->perVertex->values[i]);
      // print out the progress bar
      float cur = float(i + 1) / n;
      if (cur - progress >= 0.08f || i == n-1) {
        int pos = barWidth * cur;
        std::cout << "[";
        for (int j = 0; j < barWidth; ++j) {
          if (j < pos) std::cout << "=";
          else if (j == pos) std::cout << ">";
          else std::cout << " ";
        }
        std::cout << "] " << int(cur * 100.0) << "% \r";
        std::cout.flush();
        progress = cur;
      }
    }
    std::cout << "\nSuccessfully insert all the points and scalar values!" << std::endl;
  }

  // insert tetrahedra
  if (!inMesh->tets.empty()) {
    progress = 0.0f;
    for (size_t i = 0; i < m; ++i) {
      // print out the progress bar
      float cur = float(i + 1) / m;
      if (cur - progress >= 0.08f || i == n-1) {
        int pos = barWidth * cur;
        std::cout << "[";
        for (int j = 0; j < barWidth; ++j) {
          if (j < pos) std::cout << "=";
          else if (j == pos) std::cout << ">";
          else std::cout << " ";
        }
        std::cout << "] " << int(cur * 100.0) << "% \r";
        std::cout.flush();
        progress = cur;
      }
      auto &tet = inMesh->tets[i];
      fprintf(file, "%d %d %d %d\n", tet[0], tet[1], tet[2], tet[3]);
    }
    std::cout << "\nSuccessfully insert all the tetrahedra!" << std::endl;
  }

  fclose(file);
  
  return EXIT_SUCCESS;
}
