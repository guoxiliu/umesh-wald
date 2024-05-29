
/**
 * A tool to convert from umesh format to ts format
 * Author: Guoxi Liu (liuguoxi888@gmail.com)
 * Date: 05/26/2024
 */

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
    std::cerr << "Usage: " << argv[0] << " -o outfile.ts <infile.umesh>" << std::endl;
    return EXIT_FAILURE;
  }

  // read umesh file
  std::cout << "parsing umesh file " << inFileName << std::endl;
  umesh::UMesh::SP inMesh = umesh::io::loadBinaryUMesh(inFileName);
  std::cout << "Done reading.\n UMesh info:\n" << inMesh->toString(false) << std::endl;

  // check triangles
  if (!inMesh->triangles.empty()) {
    std::cerr << "Warning: Only tetrahedra are allowed in .ts file, triangles detected!" << std::endl;
    std::cout << "Skipping triangles" << std::endl;
    // return EXIT_FAILURE;
  }

  // check quadrangles
  if (!inMesh->quads.empty()) {
    std::cerr << "Warning: Only tetrahedra are allowed in .ts file, quadrangles detected!" << std::endl;
    std::cout << "Skipping quadrangles..." << std::endl;
    // return EXIT_FAILURE;
  }

  // check pyramids
  if (!inMesh->pyrs.empty()) {
    std::cerr << "Warning: Only tetrahedra are allowed in .ts file, pyramids detected!" << std::endl;
    std::cout << "Skipping pyramids..." << std::endl;
    // return EXIT_FAILURE;
  }

  // check wedges
  if (!inMesh->wedges.empty()) {
    std::cerr << "Warning: Only tetrahedra are allowed in .ts file, wedges detected!" << std::endl;
    std::cout << "Skipping wedges..." << std::endl;
    // return EXIT_FAILURE;
  }

  // check hexahedra
  if (!inMesh->hexes.empty()) {
    std::cerr << "Warning: Only tetrahedra are allowed in .ts file, hexahedra detected!" << std::endl;
    std::cout << "Skipping hexes..." << std::endl;
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
  fprintf(file, "%ld %ld\n", n, m);
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
