#ifndef CATMULL_CLARK_SUBDIVIDER_H
#define CATMULL_CLARK_SUBDIVIDER_H

#include "mesh/mesh.h"
#include "subdivider.h"

/**
 * @brief The CatmullClarkSubdivider class is a subdivider class that performs
 * Catmull-Clark subdivision on meshes.
 */
class CatmullClarkSubdivider : public Subdivider {
 public:
  CatmullClarkSubdivider();
  Mesh subdivide(Mesh& mesh) const override;

 private:
  void reserveSizes(Mesh& mesh, Mesh& newMesh) const;
  void geometryRefinement(Mesh& mesh, Mesh& newMesh) const;
  void topologyRefinement(Mesh& mesh, Mesh& newMesh) const;

  void setHalfEdgeData(Mesh& newMesh, int h, int edgeIdx, int vertIdx,
                       int twinIdx) const;

  QVector3D facePoint(const Face& face) const;
  QVector3D edgePoint(const HalfEdge& edge) const;
  QVector3D boundaryEdgePoint(const HalfEdge& edge) const;
  QVector3D sharpEdgePoint(const HalfEdge& edge) const;  // For crease edges
  QVector3D vertexPoint(const Vertex& vertex) const;
  QVector3D boundaryVertexPoint(const Vertex& vertex) const;
  QVector3D creaseVertexPoint(const Vertex& vertex) const;  // For vertices on creases (exactly 2 crease edges)
  int countCreaseEdges(const Vertex& vertex) const;  // Count unique crease edges incident to vertex
};

#endif  // CATMULL_CLARK_SUBDIVIDER_H
