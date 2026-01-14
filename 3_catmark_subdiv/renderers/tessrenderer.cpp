#include "tessrenderer.h"

#include <QSet>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>

#include "../mesh/face.h"
#include "../mesh/halfedge.h"
#include "../mesh/vertex.h"

namespace {

bool isRegularVertex(Vertex* v) {
  return v != nullptr && v->valence == 4 && !v->isBoundaryVertex();
}

// Collect the 9 faces of the 3x3 patch around the center face.
// Gather the 3x3 patch faces around the center quad:
// center + 4 edge neighbours + 4 corner (diagonal) neighbours.
// Returns false if any neighbour is missing or the topology is not a regular
// interior quad configuration.
bool collectPatchFaces(Face* center, QSet<Face*>& outFaces) {
  outFaces.clear();
  if (center == nullptr || center->side == nullptr || center->valence != 4)
    return false;

  auto edgeFace = [](HalfEdge* he) -> Face* {
    return (he && he->twin) ? he->twin->face : nullptr;
  };

  // Cardinal edges
  HalfEdge* e0 = center->side;             // east
  HalfEdge* e1 = e0 ? e0->next : nullptr;  // north
  HalfEdge* e2 = e1 ? e1->next : nullptr;  // west
  HalfEdge* e3 = e2 ? e2->next : nullptr;  // south
  if (!e0 || !e1 || !e2 || !e3) return false;

  Face* east  = edgeFace(e0);
  Face* north = edgeFace(e1);
  Face* west  = edgeFace(e2);
  Face* south = edgeFace(e3);
  if (!east || !north || !west || !south) return false;

  // Diagonals: step across the corner via next->twin twice
  auto diagAcross = [](HalfEdge* fromEdge, HalfEdge* nextEdge) -> Face* {
    HalfEdge* he = nextEdge;
    if (!he || !he->twin) return nullptr;
    he = he->twin->next;
    if (!he || !he->twin) return nullptr;
    return he->twin->face;
  };

  Face* ne = diagAcross(e0, e1);
  Face* nw = diagAcross(e1, e2);
  Face* sw = diagAcross(e2, e3);
  Face* se = diagAcross(e3, e0);
  if (!ne || !nw || !sw || !se) return false;

  outFaces.insert(center);
  outFaces.insert(east);
  outFaces.insert(north);
  outFaces.insert(west);
  outFaces.insert(south);
  outFaces.insert(ne);
  outFaces.insert(nw);
  outFaces.insert(sw);
  outFaces.insert(se);

  return outFaces.size() == 9;
}

struct ProjectedVertex {
  int index;
  QVector3D pos;
  float u;
  float v;
};

bool buildControlNet(Face* center, Mesh& mesh,
                     std::array<QVector3D, 16>& controlNet) {
  if (center == nullptr || center->valence != 4) return false;

  QSet<Face*> faces;
  if (!collectPatchFaces(center, faces)) return false;

  // Validate faces/vertices and collect the 16 unique vertices.
  QSet<int> vertexIds;
  for (Face* f : faces) {
    if (f == nullptr || f->valence != 4 || f->side == nullptr) return false;
    HalfEdge* e = f->side;
    for (int k = 0; k < 4; ++k) {
      if (e == nullptr || e->origin == nullptr) return false;
      Vertex* v = e->origin;
      if (!isRegularVertex(v)) return false;
      vertexIds.insert(v->index);
      e = e->next;
    }
  }

  if (vertexIds.size() != 16) return false;

  // Build a local 2D frame (u,v) on the center face for consistent ordering.
  HalfEdge* e0 = center->side;
  if (e0 == nullptr || e0->next == nullptr || e0->next->next == nullptr ||
      e0->next->next->next == nullptr)
    return false;

  QVector3D p0 = e0->origin->coords;
  QVector3D p1 = e0->next->origin->coords;
  QVector3D p2 = e0->next->next->origin->coords;
  QVector3D p3 = e0->next->next->next->origin->coords;
  QVector3D faceCenter = (p0 + p1 + p2 + p3) / 4.0f;

  QVector3D uDir = p1 - p0;
  if (uDir.lengthSquared() < 1e-10f) return false;
  uDir.normalize();

  QVector3D faceNormal = center->normal;
  if (faceNormal.lengthSquared() < 1e-10f) {
    faceNormal = QVector3D::crossProduct(p1 - p0, p2 - p0);
  }
  if (faceNormal.lengthSquared() < 1e-10f) return false;
  faceNormal.normalize();

  QVector3D vDir = QVector3D::crossProduct(faceNormal, uDir);
  if (vDir.lengthSquared() < 1e-10f) return false;
  vDir.normalize();

  std::vector<ProjectedVertex> projected;
  projected.reserve(16);
  for (int vid : vertexIds) {
    if (vid < 0 || vid >= mesh.getVertices().size()) return false;
    const QVector3D pos = mesh.getVertices()[vid].coords;
    QVector3D rel = pos - faceCenter;
    float u = QVector3D::dotProduct(rel, uDir);
    float v = QVector3D::dotProduct(rel, vDir);
    projected.push_back({vid, pos, u, v});
  }

  if (projected.size() != 16) return false;

  // Robust row grouping: sort by v, then cluster into 4 rows with tolerance;
  // sort each row by u.
  std::sort(projected.begin(), projected.end(),
            [](const ProjectedVertex& a, const ProjectedVertex& b) {
              if (std::abs(a.v - b.v) > 1e-5f) return a.v < b.v;
              return a.u < b.u;
            });

  float minV = projected.front().v;
  float maxV = projected.back().v;
  float vRange = maxV - minV;
  float vEps = vRange * 0.15f + 1e-5f;  // loose clustering

  std::vector<std::vector<ProjectedVertex>> rows;
  for (const auto& pv : projected) {
    if (rows.empty() ||
        std::abs(pv.v - rows.back().front().v) > vEps) {
      rows.push_back({});
    }
    rows.back().push_back(pv);
  }

  if (rows.size() != 4) return false;
  for (auto& row : rows) {
    std::sort(row.begin(), row.end(),
              [](const ProjectedVertex& a, const ProjectedVertex& b) {
                return a.u < b.u;
              });
    if (row.size() != 4) return false;
  }

  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      controlNet[r * 4 + c] = rows[r][c].pos;
    }
  }

  return true;
}

}  // namespace

/**
 * @brief TessellationRenderer::TessellationRenderer Creates a new tessellation
 * renderer.
 */
TessellationRenderer::TessellationRenderer() : patchVertexCount(0) {}

/**
 * @brief TessellationRenderer::~TessellationRenderer Deconstructor.
 */
TessellationRenderer::~TessellationRenderer() {
  gl->glDeleteVertexArrays(1, &vao);
  gl->glDeleteBuffers(1, &patchCoordsBO);
}

/**
 * @brief TessellationRenderer::initShaders Initializes the shaders used for the
 * Tessellation.
 */
void TessellationRenderer::initShaders() {
  tessellationShader = constructTesselationShader("patch");
}

/**
 * @brief TessellationRenderer::constructTesselationShader Constructs a shader
 * consisting of a vertex shader, tessellation control shader, tessellation
 * evaluation shader and a fragment shader. The shaders are assumed to follow
 * the naming convention: <name>.vert, <name.tesc>, <name.tese> and <name>.frag.
 * All of these files have to exist for this function to work successfully.
 * @param name Name of the shader.
 * @return The constructed shader.
 */
QOpenGLShaderProgram* TessellationRenderer::constructTesselationShader(
    const QString& name) const {
  QString pathVert = ":/shaders/" + name + ".vert";
  QString pathTesC = ":/shaders/" + name + ".tesc";
  QString pathTesE = ":/shaders/" + name + ".tese";
  QString pathFrag = ":/shaders/" + name + ".frag";
  QString pathShading = ":/shaders/shading.glsl";

  QOpenGLShaderProgram* shader = new QOpenGLShaderProgram();
  shader->addShaderFromSourceFile(QOpenGLShader::Vertex, pathVert);
  shader->addShaderFromSourceFile(QOpenGLShader::TessellationControl, pathTesC);
  shader->addShaderFromSourceFile(QOpenGLShader::TessellationEvaluation,
                                  pathTesE);
  shader->addShaderFromSourceFile(QOpenGLShader::Fragment, pathFrag);
  shader->addShaderFromSourceFile(QOpenGLShader::Fragment, pathShading);
  shader->link();
  return shader;
}

/**
 * @brief TessellationRenderer::initBuffers Initializes the buffers. The buffer
 * stores 16 control points per regular patch; no index buffer is needed.
 */
void TessellationRenderer::initBuffers() {
  gl->glGenVertexArrays(1, &vao);
  gl->glBindVertexArray(vao);

  gl->glGenBuffers(1, &patchCoordsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, patchCoordsBO);
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  gl->glBindVertexArray(0);
}

/**
 * @brief TessellationRenderer::updateBuffers Updates the buffers based on the
 * provided mesh. Only regular 3x3 patches (all valences = 4, all quads) are
 * added; irregular patches are skipped entirely.
 * @param currentMesh The mesh to update the buffer contents with.
 */
void TessellationRenderer::updateBuffers(Mesh& currentMesh) {
  std::vector<QVector3D> packedControlPoints;
  packedControlPoints.reserve(currentMesh.getFaces().size() * 16);

  for (Face& face : currentMesh.getFaces()) {
    if (face.valence != 4 || face.side == nullptr) continue;

    std::array<QVector3D, 16> net;
    if (buildControlNet(&face, currentMesh, net)) {
      packedControlPoints.insert(packedControlPoints.end(), net.begin(),
                                 net.end());
    }
  }

  gl->glBindBuffer(GL_ARRAY_BUFFER, patchCoordsBO);
  if (!packedControlPoints.empty()) {
    gl->glBufferData(GL_ARRAY_BUFFER,
                     sizeof(QVector3D) * packedControlPoints.size(),
                     packedControlPoints.data(), GL_DYNAMIC_DRAW);
  } else {
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  }

  patchVertexCount = static_cast<int>(packedControlPoints.size());
}

/**
 * @brief TessellationRenderer::updateUniforms Updates the uniforms in the
 * shader.
 */
void TessellationRenderer::updateUniforms() {
  uniModelViewMatrix = tessellationShader->uniformLocation("modelviewmatrix");
  uniProjectionMatrix = tessellationShader->uniformLocation("projectionmatrix");
  uniNormalMatrix = tessellationShader->uniformLocation("normalmatrix");
  uniUseBezier = tessellationShader->uniformLocation("useBezierPatch");
  GLint uniOuter = tessellationShader->uniformLocation("outerTessLevel");
  GLint uniInner = tessellationShader->uniformLocation("innerTessLevel");

  gl->glUniformMatrix4fv(uniModelViewMatrix, 1, false,
                         settings->modelViewMatrix.data());
  gl->glUniformMatrix4fv(uniProjectionMatrix, 1, false,
                         settings->projectionMatrix.data());
  gl->glUniformMatrix3fv(uniNormalMatrix, 1, false,
                         settings->normalMatrix.data());
  gl->glUniform1i(uniUseBezier, settings->useBezierPatch ? 1 : 0);
  if (uniOuter >= 0) gl->glUniform1f(uniOuter, 4.0f);
  if (uniInner >= 0) gl->glUniform1f(uniInner, 4.0f);
}

/**
 * @brief MeshRenderer::draw Draw call.
 */
void TessellationRenderer::draw() {
  if (patchVertexCount == 0) {
    return;  // nothing to draw (all irregular patches skipped)
  }

  tessellationShader->bind();

  if (settings->uniformUpdateRequired) {
    updateUniforms();
  }

  gl->glBindVertexArray(vao);

  gl->glPatchParameteri(GL_PATCH_VERTICES, 16);
  gl->glDrawArrays(GL_PATCHES, 0, patchVertexCount);

  gl->glBindVertexArray(0);

  tessellationShader->release();
}
