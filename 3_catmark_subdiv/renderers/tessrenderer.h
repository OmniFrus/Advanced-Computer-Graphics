#ifndef TESSRENDERER_H
#define TESSRENDERER_H

#include <QOpenGLShaderProgram>

#include "../mesh/mesh.h"
#include "renderer.h"

/**
 * @brief The TessellationRenderer class is responsible for rendering
 * Tessellated patches.
 */
class TessellationRenderer : public Renderer {
 public:
  TessellationRenderer();
  ~TessellationRenderer() override;

  void updateUniforms();
  void updateBuffers(Mesh& m);
  void draw();

 protected:
  QOpenGLShaderProgram* constructTesselationShader(const QString& name) const;
  void initShaders() override;
  void initBuffers() override;

 private:
  GLuint vao;
  GLuint patchCoordsBO;
  int patchVertexCount;
  QOpenGLShaderProgram* tessellationShader;

  // Uniforms
  GLint uniModelViewMatrix, uniProjectionMatrix, uniNormalMatrix;
  GLint uniUseBezier;
};

#endif  // TessRenderer_H
