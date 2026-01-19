#include "catmullclarksubdivider.h"

#include <QDebug>
#include <QSet>

/**
 * @brief CatmullClarkSubdivider::CatmullClarkSubdivider Creates a new empty
 * Catmull Clark subdivider.
 */
CatmullClarkSubdivider::CatmullClarkSubdivider() {}

/**
 * @brief CatmullClarkSubdivider::subdivide Subdivides the provided control mesh
 * and returns the subdivided mesh. Performs just a single subdivision step. The
 * subdivision follows the indexing rules of this paper:
 * https://diglib.eg.org/bitstream/handle/10.1111/cgf14381/v40i8pp057-070.pdf?sequence=1&isAllowed=y
 * The descriptions of the face, edge and vertex points are also quoted from
 * this paper.
 * @param controlMesh The mesh to be subdivided.
 * @return The mesh resulting of applying a single subdivision step on the
 * control mesh.
 */
Mesh CatmullClarkSubdivider::subdivide(Mesh &mesh) const {
  Mesh newMesh;
  reserveSizes(mesh, newMesh);
  geometryRefinement(mesh, newMesh);
  topologyRefinement(mesh, newMesh);
  return newMesh;
}

/**
 * @brief CatmullClarkSubdivider::reserveSizes Resizes the vertex, half-edge and
 * face vectors. Aslo recalculates the edge count.
 * @param controlMesh The control mesh.
 * @param newMesh The new mesh. At this point, the mesh is fully empty.
 */
void CatmullClarkSubdivider::reserveSizes(Mesh &controlMesh,
                                          Mesh &newMesh) const {
  int newNumEdges = 2 * controlMesh.numEdges() + controlMesh.numHalfEdges();
  int newNumFaces = controlMesh.numHalfEdges();
  int newNumHalfEdges = controlMesh.numHalfEdges() * 4;
  int newNumVerts =
      controlMesh.numVerts() + controlMesh.numFaces() + controlMesh.numEdges();

  newMesh.getVertices().resize(newNumVerts);
  newMesh.getHalfEdges().resize(newNumHalfEdges);
  newMesh.getFaces().resize(newNumFaces);
  newMesh.edgeCount = newNumEdges;
}

/**
 * @brief CatmullClarkSubdivider::geometryRefinement Performs the geometry
 * refinement. In other words, it calculates the coordinates of the vertex, edge
 * and face points. Note that this also sets the valences.
 * The valence of a new face point is equal to the valence of the face.
 * The valence of a new edge point is always 4, unless the edge point lies on a
 * boundary, in which case the valence will be 3.
 * The valence of a new vertex point is equal to the valence of the original
 * vertex point.
 * @param controlMesh The control mesh.
 * @param newMesh The new mesh. At the start of this function, the only
 * guarantee you have of this newMesh is that the vertex, half-edge and face
 * vectors have the correct sizes.
 */
void CatmullClarkSubdivider::geometryRefinement(Mesh &controlMesh,
                                                Mesh &newMesh) const {
  QVector<Vertex> &newVertices = newMesh.getVertices();
  QVector<Vertex> &vertices = controlMesh.getVertices();
  QVector<Face> &faces = controlMesh.getFaces();

  // Face Points
  for (int f = 0; f < controlMesh.numFaces(); f++) {
    QVector3D coords = facePoint(faces[f]);
    int i = controlMesh.numVerts() + faces[f].index;
    // Face points always inherit the valence of the face
    Vertex facePoint(coords, nullptr, faces[f].valence, i);
    newVertices[i] = facePoint;
  }

  // Edge Points
  QVector<HalfEdge> &halfEdges = controlMesh.getHalfEdges();
  for (int h = 0; h < controlMesh.numHalfEdges(); h++) {
    HalfEdge currentEdge = halfEdges[h];
    // Only create a new vertex per set of halfEdges (i.e. once per undirected
    // edge)
    if (h > currentEdge.twinIdx()) {
      int v = controlMesh.numVerts() + controlMesh.numFaces() +
              currentEdge.edgeIdx();
      int valence;
      QVector3D coords;
      if (currentEdge.isBoundaryEdge()) {
          coords = boundaryEdgePoint(currentEdge);
          valence = 3;
      } else if (currentEdge.isSharpEdge()) {
          // Use sharp edge rules (same as boundary) for creases
          coords = sharpEdgePoint(currentEdge);
          valence = 4;  // Sharp edges behave like boundary edges
      } else {
        coords = edgePoint(currentEdge);
        valence = 4;
      }
      newVertices[v] = Vertex(coords, nullptr, valence, v);
    }
  }

  // Vertex Points
  for (int v = 0; v < controlMesh.numVerts(); v++) {
    QVector3D coords;
    if (vertices[v].isBoundaryVertex()) {
      coords = boundaryVertexPoint(vertices[v]);
    } else {
      // Check if vertex is on a crease (has exactly 2 crease edges) or is a corner (3+ crease edges)
      int numCreaseEdges = countCreaseEdges(vertices[v]);
      if (numCreaseEdges >= 3) {
        // Corner: position unchanged
        coords = vertices[v].coords;
      } else if (numCreaseEdges == 2) {
        //Standard crease vertex: use crease vertex rules
        coords = creaseVertexPoint(vertices[v]);
      } else {
        // Smooth vertex (0 or 1 crease edge): use smooth vertex rules
        coords = vertexPoint(vertices[v]);
      }
    }
    newVertices[v] = Vertex(coords, nullptr, vertices[v].valence, v);
  }
}

/**
 * @brief CatmullClarkSubdivider::vertexPoint Calculates the new position of the
 * provided vertex. It does so according to the formula for smooth vertex
 * points:
 * Q/n + 2R/n + S(n-3)/n
 * where
 * Q = the average of the new face points of all faces adjacent to the old
 * vertex point.
 * R = the average of the midpoints of all edges incident to the
 * old vertex point.
 * S = old vertex point.
 * n = valence of the vertex.
 * @param vertex The vertex to calculate the new position of. Note that this
 * vertex is the vertex from the control mesh.
 * @return The coordinates of the new vertex point.
 */
QVector3D CatmullClarkSubdivider::vertexPoint(const Vertex &vertex) const {
  // For smooth vertices (0 or 1 sharp edges), use the standard Catmull-Clark
  // vertex rule with FULL valence (all incident edges/faces), as per the paper.
  // This is the standard smooth vertex rule (Eq. 2) which includes all edges.
  HalfEdge *edge = vertex.out;
  QVector3D R;  // average of all edge mid points
  QVector3D Q;  // average of all face points adjacent to the vertex
  
  for (int i = 0; i < vertex.valence; i++) {
    // Include ALL edges (both smooth and crease) for the smooth vertex rule
    R += (edge->origin->coords + edge->next->origin->coords) / 2.0;
    Q += facePoint(*edge->face);
    edge = edge->prev->twin;
  }
  
  float n = float(vertex.valence);
  Q /= n;
  R /= n;
  // Standard Catmull-Clark vertex mask using full valence (Eq. 2)
  return (Q + 2 * R + (vertex.coords * (n - 3.0f))) / n;
}

/**
 * @brief CatmullClarkSubdivider::boundaryVertexPoint Calculates the new
 * position of the provided vertex. It does so according to the formula for
 * boundary vertex points:
 * (R + S) / 2
 * where
 * R = the average of the midpoints of all edges incident to the
 * old vertex point.
 * S = old vertex point.
 * @param vertex The vertex to calculate the new position of. Note that this
 * vertex is the vertex from the control mesh.
 * @return The coordinates of the new boundary vertex point.
 */
QVector3D CatmullClarkSubdivider::boundaryVertexPoint(
    const Vertex &vertex) const {
  QVector3D boundPoint = vertex.coords * 2;
  boundPoint += boundaryEdgePoint(*vertex.nextBoundaryHalfEdge());
  boundPoint += boundaryEdgePoint(*vertex.prevBoundaryHalfEdge());
  return boundPoint / 4.0;
}

/**
 * @brief CatmullClarkSubdivider::creaseVertexPoint Calculates the new position
 * of a vertex on a crease. Uses the same formula as boundary vertices:
 * (R + S) / 2 where R is the average of crease edge midpoints and S is the
 * vertex position.
 * @param vertex The vertex on a crease to calculate the new position of.
 * @return The coordinates of the new crease vertex point.
 */
int CatmullClarkSubdivider::countCreaseEdges(const Vertex& vertex) const {
  // Count the number of unique crease edges incident to this vertex
  QSet<int> creaseEdgeIndices;  // Use edge indices to avoid duplicates
  
  HalfEdge* h = vertex.out;
  int maxIterations = vertex.valence * 2;  // Safety limit
  int iterations = 0;
  
  // Iterate around the vertex - each edge will be encountered exactly once
  // as an outgoing half-edge from this vertex
  do {
    if (h->isSharpEdge()) {
      creaseEdgeIndices.insert(h->edgeIndex);
    }
    // Move to next edge around vertex
    h = h->prev->twin;
    if (h == nullptr) break;  // Reached boundary
    iterations++;
  } while (h != vertex.out && h != nullptr && iterations < maxIterations);
  
  return creaseEdgeIndices.size();
}

QVector3D CatmullClarkSubdivider::creaseVertexPoint(
    const Vertex &vertex) const {
  // This function should only be called for vertices with exactly 2 crease edges
  // Formula: v^(i+1) = (e_j^i + 6v^i + e_k^i) / 8
  // where e_j and e_k are the midpoints of the two crease edges
  
  // Find the two crease edges
  HalfEdge* creaseEdge1 = nullptr;
  HalfEdge* creaseEdge2 = nullptr;
  QSet<int> foundEdgeIndices;
  
  HalfEdge* h = vertex.out;
  int maxIterations = vertex.valence * 2;
  int iterations = 0;
  
  do {
    if (h->isSharpEdge() && !foundEdgeIndices.contains(h->edgeIndex)) {
      if (creaseEdge1 == nullptr) {
        creaseEdge1 = h;
        foundEdgeIndices.insert(h->edgeIndex);
      } else if (creaseEdge2 == nullptr) {
        creaseEdge2 = h;
        foundEdgeIndices.insert(h->edgeIndex);
        break;
      }
    }
    h = h->prev->twin;
    if (h == nullptr) break;
    iterations++;
  } while (h != vertex.out && h != nullptr && iterations < maxIterations);
  
  if (creaseEdge1 != nullptr && creaseEdge2 != nullptr) {
    QVector3D result = 0.5f*vertex.coords;
    result += 0.25f*sharpEdgePoint(*creaseEdge1);
    result += 0.25f*sharpEdgePoint(*creaseEdge2);
    return result;
  }
  // Fallback: should not happen, but return vertex position unchanged
  return vertex.coords;
}

/**
 * @brief CatmullClarkSubdivider::edgePoint Calculates the position of the edge
 * point according to the formula for smooth edge points:
 * (M + Q) / 2
 * where
 * Q = the average of the new face points of the two faces adjacent to the old
 * edge.
 * M = the midpoint of the edge.
 * @param edge One of the half-edges that lives on the edge to calculate
 * the edge point. Note that this half-edge is the half-edge from the control
 * mesh.
 * @return The coordinates of the new edge point.
 */
QVector3D CatmullClarkSubdivider::edgePoint(const HalfEdge &edge) const {
  QVector3D edgePt = boundaryEdgePoint(edge);
  edgePt += (facePoint(*edge.face) + facePoint(*edge.twin->face)) / 2.0;
  return edgePt /= 2.0;
}

/**
 * @brief CatmullClarkSubdivider::boundaryEdgePoint Calculates the position of
 * the boundary edge point by taking the midpoint of the edge.
 * @param edge One of the half-edges that lives on the edge to calculate
 * the edge point. Note that this half-edge is the half-edge from the control
 * mesh.
 * @return The coordinates of the new boundary edge point.
 */
QVector3D CatmullClarkSubdivider::boundaryEdgePoint(
    const HalfEdge &edge) const {
  return (edge.origin->coords + edge.next->origin->coords) / 2.0f;
}

/**
 * @brief CatmullClarkSubdivider::sharpEdgePoint Calculates the position of a
 * sharp edge point (crease) by taking the midpoint of the edge. This is the
 * same as boundary edge point - sharp edges use boundary-like rules.
 * @param edge One of the half-edges that lives on the sharp edge.
 * @return The coordinates of the new sharp edge point.
 */
QVector3D CatmullClarkSubdivider::sharpEdgePoint(
    const HalfEdge &edge) const {
  return (edge.origin->coords + edge.next->origin->coords) / 2.0f;
}

/**
 * @brief CatmullClarkSubdivider::facePoint Calculates the position of the face
 * point by averaging the positions of all vertices adjacent to the face.
 * @param face The face to calculate the face point of. Note that this face is
 * the face from the control mesh.
 * @return The coordinates of the new face point.
 */
QVector3D CatmullClarkSubdivider::facePoint(const Face &face) const {
  QVector3D edgePt;
  HalfEdge *edge = face.side;
  for (int side = 0; side < face.valence; side++) {
    edgePt += edge->origin->coords;
    edge = edge->next;
  }
  return edgePt / face.valence;
}

/**
 * @brief CatmullClarkSubdivider::topologyRefinement Performs the topology
 * refinement. Every face is split into n new faces, where n is the valence of
 * the original face. Newly generated faces are all quads.
 * @param controlMesh The control mesh.
 * @param newMesh The new mesh.
 */
void CatmullClarkSubdivider::topologyRefinement(Mesh &controlMesh,
                                                Mesh &newMesh) const {
  for (int f = 0; f < newMesh.numFaces(); ++f) {
    newMesh.faces[f].index = f;
    newMesh.faces[f].valence = 4;
  }

  // Split halfedges
  for (int h = 0; h < controlMesh.numHalfEdges(); ++h) {
    HalfEdge *edge = &controlMesh.halfEdges[h];

    int h1 = 4 * h;
    int h2 = 4 * h + 1;
    int h3 = 4 * h + 2;
    int h4 = 4 * h + 3;

    int twinIdx1 = edge->twinIdx() < 0 ? -1 : 4 * edge->twin->next->index + 3;
    int twinIdx2 = 4 * edge->next->index + 2;
    int twinIdx3 = 4 * edge->prev->index + 1;
    int twinIdx4 = 4 * edge->prev->twinIdx();

    int vertIdx1 = edge->origin->index;
    int vertIdx2 =
        controlMesh.numVerts() + controlMesh.numFaces() + edge->edgeIndex;
    int vertIdx3 = controlMesh.numVerts() + edge->faceIdx();
    int vertIdx4 =
        controlMesh.numVerts() + controlMesh.numFaces() + edge->prev->edgeIndex;

    int edgeIdx1 = 2 * edge->edgeIndex + (h > edge->twinIdx() ? 0 : 1);
    int edgeIdx2 = 2 * controlMesh.numEdges() + h;
    int edgeIdx3 = 2 * controlMesh.numEdges() + edge->prev->index;
    int edgeIdx4 = 2 * edge->prev->edgeIndex +
                   (edge->prevIdx() > edge->prev->twinIdx() ? 1 : 0);

    setHalfEdgeData(newMesh, h1, edgeIdx1, vertIdx1, twinIdx1);
    setHalfEdgeData(newMesh, h2, edgeIdx2, vertIdx2, twinIdx2);
    setHalfEdgeData(newMesh, h3, edgeIdx3, vertIdx3, twinIdx3);
    setHalfEdgeData(newMesh, h4, edgeIdx4, vertIdx4, twinIdx4);
    
    // Propagate sharpness according to hybrid subdivision rules
    // When an edge with sharpness s > 0 is subdivided:
    // - The two child edges along the original edge get sharpness s-1
    // - New edges connecting to face points are smooth (sharpness 0)
    
    // Calculate new sharpness for the original edge (decrement by 1)
    int newSharpness = 0;
    if (edge->sharpness > 0) {
      newSharpness = edge->sharpness - 1;
    } else if (edge->sharpness == -1) {
      newSharpness = -1;  // Infinite sharpness stays infinite
    }
    
    // h1 is part of the original edge (from vertex to edge point)
    // This edge inherits the decremented sharpness
    newMesh.halfEdges[h1].sharpness = newSharpness;
    if (twinIdx1 >= 0) {
      // Set twin's sharpness too (both halves of an edge must have same sharpness)
      newMesh.halfEdges[twinIdx1].sharpness = newSharpness;
    }
    
    // h4 is part of the previous edge (from previous edge point to vertex)
    int prevEdgeSharpness = 0;
    if (edge->prev->sharpness > 0) {
      prevEdgeSharpness = edge->prev->sharpness - 1;
    } else if (edge->prev->sharpness == -1) {
      prevEdgeSharpness = -1;  // Infinite sharpness stays infinite
    }
    newMesh.halfEdges[h4].sharpness = prevEdgeSharpness;
    if (twinIdx4 >= 0) {
      newMesh.halfEdges[twinIdx4].sharpness = prevEdgeSharpness;
    }
    
    // h2 and h3 are new edges connecting to face points - they are always smooth
    newMesh.halfEdges[h2].sharpness = 0;
    newMesh.halfEdges[h3].sharpness = 0;
    // Set twins if they exist (new edges should also be smooth)
    if (newMesh.halfEdges[h2].twin) {
      newMesh.halfEdges[h2].twin->sharpness = 0;
    }
    if (newMesh.halfEdges[h3].twin) {
      newMesh.halfEdges[h3].twin->sharpness = 0;
    }
  }
}

/**
 * @brief LoopSubdivider::setHalfEdgeData Sets the data of a single half-edge
 * (and the corresponding vertex and face)
 * @param newMesh The new mesh this half-edge will live in.
 * @param h Index of the half-edge.
 * @param edgeIdx Index of the (undirected) edge this half-edge will belong to.
 * @param vertIdx Index of the vertex that this half-edge will originate from.
 * @param twinIdx Index of the twin of this half-edge. -1 if the half-edge lies
 * on a boundary.
 */
void CatmullClarkSubdivider::setHalfEdgeData(Mesh &newMesh, int h, int edgeIdx,
                                             int vertIdx, int twinIdx) const {
  HalfEdge *halfEdge = &newMesh.halfEdges[h];

  halfEdge->edgeIndex = edgeIdx;
  halfEdge->index = h;
  halfEdge->origin = &newMesh.vertices[vertIdx];
  halfEdge->face = &newMesh.faces[halfEdge->faceIdx()];
  halfEdge->next = &newMesh.halfEdges[halfEdge->nextIdx()];
  halfEdge->prev = &newMesh.halfEdges[halfEdge->prevIdx()];
  halfEdge->twin = twinIdx < 0 ? nullptr : &newMesh.halfEdges[twinIdx];

  halfEdge->origin->out = halfEdge;
  halfEdge->origin->index = vertIdx;
  halfEdge->face->side = halfEdge;
}
