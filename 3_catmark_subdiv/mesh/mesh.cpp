#include "mesh.h"

#include <assert.h>
#include <math.h>
#include <cmath> // for cosf, M_PI, etc.

#include <QDebug>
#include <QSet>

/**
 * @brief Mesh::Mesh Initializes an empty mesh.
 */
Mesh::Mesh() {}

/**
 * @brief Mesh::~Mesh Deconstructor. Clears all the data of the half-edge data.
 */
Mesh::~Mesh() {
  vertices.clear();
  vertices.squeeze();
  halfEdges.clear();
  halfEdges.squeeze();
  faces.clear();
  faces.squeeze();
}

/**
 * @brief Mesh::recalculateNormals Recalculates the face and vertex normals.
 */
void Mesh::recalculateNormals() {
  for (int f = 0; f < numFaces(); f++) {
    faces[f].recalculateNormal();
  }

  vertexNormals.clear();
  vertexNormals.fill({0, 0, 0}, numVerts());

  // normal computation
  for (int h = 0; h < numHalfEdges(); ++h) {
    HalfEdge* edge = &halfEdges[h];
    QVector3D pPrev = edge->prev->origin->coords;
    QVector3D pCur = edge->origin->coords;
    QVector3D pNext = edge->next->origin->coords;

    QVector3D edgeA = (pPrev - pCur);
    QVector3D edgeB = (pNext - pCur);

    float edgeLengths = edgeA.length() * edgeB.length();
    float edgeDot = QVector3D::dotProduct(edgeA, edgeB) / edgeLengths;
    float angle = sqrt(1 - edgeDot * edgeDot);

    vertexNormals[edge->origin->index] +=
        (angle * edge->face->normal) / edgeLengths;
  }

  for (int v = 0; v < numVerts(); ++v) {
    vertexNormals[v] /= vertexNormals[v].length();
  }
}

// Global/Static flag to control limit position extraction (set by the UI at runtime)
bool g_showLimitPosition = false;
bool g_prevShowLimitPosition = false;

/**
 * @brief Mesh::extractAttributes Extracts the normals, vertex coordinates and
 * indices into easy-to-access buffers.
 * @param selectedEdge Optional pointer to the currently selected edge for highlighting
 * @param selectedVertex Optional pointer to the currently selected vertex for highlighting
 */
void Mesh::extractAttributes(HalfEdge* selectedEdge, Vertex* selectedVertex) {
    if (g_showLimitPosition && !g_prevShowLimitPosition) {
        backupOriginalCoordsIfNeeded();
        projectVerticesToCatmullClarkLimit();
    }
    if (!g_showLimitPosition && g_prevShowLimitPosition) {
        restoreOriginalCoords();
    }
    g_prevShowLimitPosition = g_showLimitPosition;
    recalculateNormals();

  vertexCoords.clear();
  vertexCoords.reserve(vertices.size());
  for (int v = 0; v < vertices.size(); v++) {
    vertexCoords.append(vertices[v].coords);
  }

  polyIndices.clear();
  polyIndices.reserve(halfEdges.size());
  for (int f = 0; f < faces.size(); f++) {
    HalfEdge* currentEdge = faces[f].side;
    for (int m = 0; m < faces[f].valence; m++) {
      polyIndices.append(currentEdge->origin->index);
      currentEdge = currentEdge->next;
    }
    // append MAX_INT to signify end of face
    polyIndices.append(INT_MAX);
  }
  polyIndices.squeeze();

  quadIndices.clear();
  quadIndices.reserve(halfEdges.size() + faces.size());
  for (int k = 0; k < faces.size(); k++) {
    Face* face = &faces[k];
    HalfEdge* currentEdge = face->side;
    if (face->valence == 4) {
      for (int m = 0; m < face->valence; m++) {
        quadIndices.append(currentEdge->origin->index);
        currentEdge = currentEdge->next;
      }
    }
  }
  quadIndices.squeeze();
  
  // Extract edge data for visualization
  extractEdgeData(selectedEdge);
  
  // Extract vertex data for visualization
  extractVertexData(selectedVertex);
}

/**
 * @brief Mesh::numVerts Retrieves the number of vertices.
 * @return The number of vertices.
 */
int Mesh::numVerts() { return vertices.size(); }

/**
 * @brief Mesh::numHalfEdges Retrieves the number of half-edges.
 * @return The number of half-edges.
 */
int Mesh::numHalfEdges() { return halfEdges.size(); }

/**
 * @brief Mesh::numFaces Retrieves the number of faces.
 * @return The number of faces.
 */
int Mesh::numFaces() { return faces.size(); }

/**
 * @brief Mesh::numEdges Retrieves the number of edges.
 * @return The number of edges.
 */
int Mesh::numEdges() { return edgeCount; }

void Mesh::backupOriginalCoordsIfNeeded() {
    if (originalCoords.isEmpty()) {
        originalCoords.reserve(vertices.size());
        for (const Vertex &v : vertices) {
            originalCoords.append(v.coords);
        }
    }
}

void Mesh::restoreOriginalCoords() {
    if (originalCoords.size() == vertices.size()) {
        for (int i = 0; i < vertices.size(); ++i) {
            vertices[i].coords = originalCoords[i];
        }
        originalCoords.clear();
    }
}

void Mesh::projectVerticesToCatmullClarkLimit() {
    // Assumes backup has already been done!
    QVector<QVector3D> limitPositions(vertices.size());
    for (int vi = 0; vi < vertices.size(); ++vi) {
        Vertex &v = vertices[vi];
        if (v.isBoundaryVertex()) {
            // Boundary: p_limit = (1/6)*p_prev + (4/6)*v + (1/6)*p_next
            HalfEdge* nextB = v.nextBoundaryHalfEdge();
            HalfEdge* prevB = v.prevBoundaryHalfEdge();
            QVector3D V = v.coords;
            QVector3D Vnext = nextB->next->origin->coords;
            QVector3D Vprev = prevB->origin->coords;
            limitPositions[vi] = (Vprev + 4.0f * V + Vnext) / 6.0f;
        } else {
            // Interior: Loop et al. 2009 / Halstead et al.
            int n = v.valence;
            float theta = 2 * M_PI / n;
            float c = cosf(theta);
            float w = (5.0f/8.0f) - powf((3.0f + 2.0f*c)/8.0f, 2);
            w /= n;

            QVector3D neighborSum(0,0,0);
            HalfEdge* h = v.out;
            for (int j = 0; j < n; ++j) {
                neighborSum += h->next->origin->coords;
                h = h->prev->twin;
            }
            limitPositions[vi] = (1.0f - w*n) * v.coords + w * neighborSum;
        }
    }
    for (int vi = 0; vi < vertices.size(); ++vi) {
        vertices[vi].coords = limitPositions[vi];
    }
}

/**
 * @brief Mesh::setCreaseEdge Sets the sharpness value for an edge connecting
 * two vertices. This is a utility function for testing semi-sharp creases.
 * @param vertexIdx1 Index of the first vertex.
 * @param vertexIdx2 Index of the second vertex.
 * @param sharpness The sharpness value (0 = smooth, >0 = crease, can be non-integer, -1 = infinite).
 */
void Mesh::setCreaseEdge(int vertexIdx1, int vertexIdx2, float sharpness) {
  // Find all half-edges connecting these two vertices
  for (int h = 0; h < halfEdges.size(); ++h) {
    HalfEdge* edge = &halfEdges[h];
    if (edge->origin->index == vertexIdx1 && 
        edge->next->origin->index == vertexIdx2) {
      // Found an edge from vertexIdx1 to vertexIdx2
      edge->sharpness = sharpness;
      // Also set the twin's sharpness
      if (edge->twin) {
        edge->twin->sharpness = sharpness;
      }
    } else if (edge->origin->index == vertexIdx2 && 
               edge->next->origin->index == vertexIdx1) {
      // Found an edge from vertexIdx2 to vertexIdx1
      edge->sharpness = sharpness;
      // Also set the twin's sharpness
      if (edge->twin) {
        edge->twin->sharpness = sharpness;
      }
    }
  }
}

/**
 * @brief Mesh::extractEdgeData Extracts edge coordinates and colors based on
 * sharpness for visualization. Red = sharp edge, Yellow = smooth edge.
 * @param selectedEdge Optional pointer to the currently selected edge for highlighting
 */
void Mesh::extractEdgeData(HalfEdge* selectedEdge) {
  edgeCoords.clear();
  edgeColors.clear();
  
  // Use a set to avoid drawing each edge twice (since we have half-edges)
  QSet<QPair<int, int>> processedEdges;
  
  for (int h = 0; h < halfEdges.size(); ++h) {
    HalfEdge* edge = &halfEdges[h];
    int v1 = edge->origin->index;
    int v2 = edge->next->origin->index;
    
    // Create a canonical edge representation (smaller index first)
    QPair<int, int> edgePair = (v1 < v2) ? QPair<int, int>(v1, v2) : QPair<int, int>(v2, v1);
    
    // Only process each edge once
    if (!processedEdges.contains(edgePair)) {
      processedEdges.insert(edgePair);
      
      // Add edge coordinates
      edgeCoords.append(vertices[v1].coords);
      edgeCoords.append(vertices[v2].coords);
      
      // Check if this is the selected edge (check both half-edges)
      bool isSelected = (selectedEdge != nullptr && 
                         (edge == selectedEdge || edge->twin == selectedEdge));
      
      QVector3D color;
      if (isSelected) {
        // Highlight selected edge with bright cyan
        color = QVector3D(0.0f, 1.0f, 1.0f);  // Cyan for selected edge
      } else {
        // Determine color based on sharpness
        float s = edge->sharpness;
        if (s == -1.0f) {
          // Infinite sharpness: bright red
          color = QVector3D(1.0f, 0.0f, 0.0f);
        } else if (s > 0.0f) {
          // Semi-sharp or sharp: interpolate from red to yellow based on sharpness
          float normalized = fmin(fmax(s, 0.0f), 5.0f) / 5.0f;
          color = QVector3D(1.0f, (1.0f - normalized), 0.0f);
        } else {
          // Smooth edge: yellow
          color = QVector3D(1.0f, 1.0f, 0.0f);
        }
      }
      
      // Add color for both vertices of the edge
      edgeColors.append(color);
      edgeColors.append(color);
    }
  }
  
  edgeCoords.squeeze();
  edgeColors.squeeze();
}

/**
 * @brief Mesh::extractVertexData Extracts vertex coordinates and colors based on
 * whether they are boundary vertices. Blue = boundary vertex, Green = normal vertex.
 * @param selectedVertex Optional pointer to the currently selected vertex for highlighting
 */
void Mesh::extractVertexData(Vertex* selectedVertex) {
  vertexDisplayCoords.clear();
  vertexDisplayColors.clear();
  
  for (int v = 0; v < vertices.size(); ++v) {
    Vertex* vertex = &vertices[v];
    
    // Add vertex coordinates
    vertexDisplayCoords.append(vertex->coords);
    
    // Check if this is the selected vertex
    bool isSelected = (selectedVertex != nullptr && vertex == selectedVertex);
    
    QVector3D color;
    if (isSelected) {
      // Highlight selected vertex with bright magenta
      color = QVector3D(1.0f, 0.0f, 1.0f);  // Magenta for selected vertex
    } else if (vertex->isBoundaryVertex()) {
      color = QVector3D(0.0f, 0.0f, 1.0f);  // Blue for boundary vertices
    } else {
      color = QVector3D(0.0f, 1.0f, 0.0f);  // Green for normal vertices
    }
    
    vertexDisplayColors.append(color);
  }
  
  vertexDisplayCoords.squeeze();
  vertexDisplayColors.squeeze();
}
