#ifndef MESH_H
#define MESH_H

#include <QVector>

#include "face.h"
#include "halfedge.h"
#include "vertex.h"

/**
 * @brief The Mesh class Representation of a mesh using the half-edge data
 * structure.
 */
class Mesh {
 public:
  Mesh();
  ~Mesh();

  inline QVector<Vertex>& getVertices() { return vertices; }
  inline QVector<HalfEdge>& getHalfEdges() { return halfEdges; }
  inline QVector<Face>& getFaces() { return faces; }

  inline QVector<QVector3D>& getVertexCoords() { return vertexCoords; }
  inline QVector<QVector3D>& getVertexNorms() { return vertexNormals; }
  inline QVector<unsigned int>& getPolyIndices() { return polyIndices; }
  inline QVector<unsigned int>& getQuadIndices() { return quadIndices; }
  inline QVector<QVector3D>& getEdgeCoords() { return edgeCoords; }
  inline QVector<QVector3D>& getEdgeColors() { return edgeColors; }
  inline QVector<QVector3D>& getVertexDisplayCoords() { return vertexDisplayCoords; }
  inline QVector<QVector3D>& getVertexDisplayColors() { return vertexDisplayColors; }

  void extractAttributes();
  void recalculateNormals();
  void projectVerticesToCatmullClarkLimit();

  int numVerts();
  int numHalfEdges();
  int numFaces();
  int numEdges();

  void backupOriginalCoordsIfNeeded();
  void restoreOriginalCoords();

  // Utility function to set crease edges (for testing semi-sharp creases)
  // Sets sharpness for all half-edges of edges connecting the specified vertices
  void setCreaseEdge(int vertexIdx1, int vertexIdx2, int sharpness);

 private:
  void extractEdgeData();  // Extracts edge coordinates and colors for visualization
  void extractVertexData();  // Extracts vertex coordinates and colors for visualization
  QVector<QVector3D> vertexCoords;
  QVector<QVector3D> vertexNormals;
  QVector<unsigned int> polyIndices;
  // for quad tessellation
  QVector<unsigned int> quadIndices;
  QVector<QVector3D> originalCoords;
  // for edge visualization
  QVector<QVector3D> edgeCoords;
  QVector<QVector3D> edgeColors;
  // for vertex visualization
  QVector<QVector3D> vertexDisplayCoords;
  QVector<QVector3D> vertexDisplayColors;

  QVector<Vertex> vertices;
  QVector<Face> faces;
  QVector<HalfEdge> halfEdges;

  int edgeCount;

  // These classes require access to the private fields to prevent a bunch of
  // function calls.
  friend class MeshInitializer;
  friend class Subdivider;
  friend class CatmullClarkSubdivider;
};

#endif  // MESH_H
