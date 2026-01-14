#ifndef MESHRENDERER_H
#define MESHRENDERER_H

#include <QOpenGLShaderProgram>

#include "../mesh/mesh.h"
#include "renderer.h"

/**
 * @brief The MeshRenderer class is responsible for rendering a mesh. Can render
 * any arbitrary mesh.
 */
class MeshRenderer : public Renderer {
 public:
  MeshRenderer();
  ~MeshRenderer() override;

  void updateUniforms();
  void updateBuffers(Mesh& m);
  void draw();

 protected:
  void initShaders() override;
  void initBuffers() override;

 private:
  GLuint vao;
  GLuint meshCoordsBO, meshNormalsBO, meshIndexBO;
  int meshIBOSize;

  // Edge rendering buffers
  GLuint edgeVAO;
  GLuint edgeCoordsBO, edgeColorsBO;
  int edgeVertexCount;

  // Vertex rendering buffers
  GLuint vertexVAO;
  GLuint vertexCoordsBO, vertexColorsBO;
  int vertexDisplayCount;

  // Uniforms
  GLint uniModelViewMatrix, uniProjectionMatrix, uniNormalMatrix;
  
  // Edge shader (also used for vertex rendering)
  QOpenGLShaderProgram* edgeShader;
};

#endif  // MESHRENDERER_H
