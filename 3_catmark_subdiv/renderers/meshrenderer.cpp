#include "meshrenderer.h"

/**
 * @brief MeshRenderer::MeshRenderer Creates a new mesh renderer.
 */
MeshRenderer::MeshRenderer() : meshIBOSize(0), edgeVertexCount(0), vertexDisplayCount(0), edgeShader(nullptr) {}

/**
 * @brief MeshRenderer::~MeshRenderer Deconstructor.
 */
MeshRenderer::~MeshRenderer() {
  gl->glDeleteVertexArrays(1, &vao);
  gl->glDeleteVertexArrays(1, &edgeVAO);
  gl->glDeleteVertexArrays(1, &vertexVAO);

  gl->glDeleteBuffers(1, &meshCoordsBO);
  gl->glDeleteBuffers(1, &meshNormalsBO);
  gl->glDeleteBuffers(1, &meshIndexBO);
  gl->glDeleteBuffers(1, &edgeCoordsBO);
  gl->glDeleteBuffers(1, &edgeColorsBO);
  gl->glDeleteBuffers(1, &vertexCoordsBO);
  gl->glDeleteBuffers(1, &vertexColorsBO);
  
  if (edgeShader) {
    delete edgeShader;
  }
}

/**
 * @brief MeshRenderer::initShaders Initializes the shaders used to shade a
 * mesh.
 */
void MeshRenderer::initShaders() {
  shaders.insert(ShaderType::PHONG, constructDefaultShader("phong"));
  // Create edge shader separately (not in shaders map to avoid conflicts)
  QString pathVert = ":/shaders/edge.vert";
  QString pathFrag = ":/shaders/edge.frag";
  edgeShader = new QOpenGLShaderProgram();
  edgeShader->addShaderFromSourceFile(QOpenGLShader::Vertex, pathVert);
  edgeShader->addShaderFromSourceFile(QOpenGLShader::Fragment, pathFrag);
  edgeShader->link();
}

/**
 * @brief MeshRenderer::initBuffers Initializes the buffers. Uses indexed
 * rendering. The coordinates and normals are passed into the shaders.
 */
void MeshRenderer::initBuffers() {
  gl->glGenVertexArrays(1, &vao);
  gl->glBindVertexArray(vao);

  gl->glGenBuffers(1, &meshCoordsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, meshCoordsBO);
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  gl->glGenBuffers(1, &meshNormalsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, meshNormalsBO);
  gl->glEnableVertexAttribArray(1);
  gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  gl->glGenBuffers(1, &meshIndexBO);
  gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIndexBO);

  gl->glBindVertexArray(0);
  
  // Initialize edge rendering buffers
  gl->glGenVertexArrays(1, &edgeVAO);
  gl->glBindVertexArray(edgeVAO);
  
  gl->glGenBuffers(1, &edgeCoordsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, edgeCoordsBO);
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  gl->glGenBuffers(1, &edgeColorsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, edgeColorsBO);
  gl->glEnableVertexAttribArray(1);
  gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  gl->glBindVertexArray(0);
  
  // Initialize vertex rendering buffers
  gl->glGenVertexArrays(1, &vertexVAO);
  gl->glBindVertexArray(vertexVAO);
  
  gl->glGenBuffers(1, &vertexCoordsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, vertexCoordsBO);
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  gl->glGenBuffers(1, &vertexColorsBO);
  gl->glBindBuffer(GL_ARRAY_BUFFER, vertexColorsBO);
  gl->glEnableVertexAttribArray(1);
  gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  
  gl->glBindVertexArray(0);
}

/**
 * @brief MeshRenderer::updateBuffers Updates the buffers based on the provided
 * mesh.
 * @param mesh The mesh to update the buffer contents with.
 */
void MeshRenderer::updateBuffers(Mesh& mesh) {
  QVector<QVector3D>& vertexCoords = mesh.getVertexCoords();
  QVector<QVector3D>& vertexNormals = mesh.getVertexNorms();
  QVector<unsigned int>& polyIndices = mesh.getPolyIndices();

  gl->glBindBuffer(GL_ARRAY_BUFFER, meshCoordsBO);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * vertexCoords.size(),
                   vertexCoords.data(), GL_STATIC_DRAW);

  gl->glBindBuffer(GL_ARRAY_BUFFER, meshNormalsBO);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * vertexNormals.size(),
                   vertexNormals.data(), GL_STATIC_DRAW);

  gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIndexBO);
  gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(unsigned int) * polyIndices.size(),
                   polyIndices.data(), GL_STATIC_DRAW);

  meshIBOSize = polyIndices.size();
  
  // Update edge buffers
  QVector<QVector3D>& edgeCoords = mesh.getEdgeCoords();
  QVector<QVector3D>& edgeColors = mesh.getEdgeColors();
  
  gl->glBindVertexArray(edgeVAO);
  
  gl->glBindBuffer(GL_ARRAY_BUFFER, edgeCoordsBO);
  if (!edgeCoords.empty()) {
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * edgeCoords.size(),
                     edgeCoords.data(), GL_STATIC_DRAW);
  } else {
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
  }
  
  gl->glBindBuffer(GL_ARRAY_BUFFER, edgeColorsBO);
  if (!edgeColors.empty()) {
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * edgeColors.size(),
                     edgeColors.data(), GL_STATIC_DRAW);
  } else {
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
  }
  
  edgeVertexCount = edgeCoords.size();
  
  gl->glBindVertexArray(0);
  
  // Update vertex display buffers
  QVector<QVector3D>& vertexDisplayCoords = mesh.getVertexDisplayCoords();
  QVector<QVector3D>& vertexDisplayColors = mesh.getVertexDisplayColors();
  
  gl->glBindVertexArray(vertexVAO);
  
  gl->glBindBuffer(GL_ARRAY_BUFFER, vertexCoordsBO);
  if (!vertexDisplayCoords.empty()) {
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * vertexDisplayCoords.size(),
                     vertexDisplayCoords.data(), GL_STATIC_DRAW);
  } else {
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
  }
  
  gl->glBindBuffer(GL_ARRAY_BUFFER, vertexColorsBO);
  if (!vertexDisplayColors.empty()) {
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D) * vertexDisplayColors.size(),
                     vertexDisplayColors.data(), GL_STATIC_DRAW);
  } else {
    gl->glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
  }
  
  vertexDisplayCount = vertexDisplayCoords.size();
  
  gl->glBindVertexArray(0);
}

/**
 * @brief MeshRenderer::updateUniforms Updates the uniforms in the shader.
 */
void MeshRenderer::updateUniforms() {
  QOpenGLShaderProgram* shader = shaders[settings->currentShader];

  uniModelViewMatrix = shader->uniformLocation("modelviewmatrix");
  uniProjectionMatrix = shader->uniformLocation("projectionmatrix");
  uniNormalMatrix = shader->uniformLocation("normalmatrix");

  gl->glUniformMatrix4fv(uniModelViewMatrix, 1, false,
                         settings->modelViewMatrix.data());
  gl->glUniformMatrix4fv(uniProjectionMatrix, 1, false,
                         settings->projectionMatrix.data());
  gl->glUniformMatrix3fv(uniNormalMatrix, 1, false,
                         settings->normalMatrix.data());
}

/**
 * @brief MeshRenderer::draw Draw call.
 */
void MeshRenderer::draw() {
  shaders[settings->currentShader]->bind();

  if (settings->uniformUpdateRequired) {
    updateUniforms();
  }
  // enable primitive restart to allow for drawing faces of arbitrary valence
  gl->glEnable(GL_PRIMITIVE_RESTART);
  gl->glPrimitiveRestartIndex(INT_MAX);

  gl->glBindVertexArray(vao);

  if (settings->wireframeMode) {
    gl->glDrawElements(GL_LINE_LOOP, meshIBOSize, GL_UNSIGNED_INT, nullptr);
  } else {
    gl->glDrawElements(GL_TRIANGLE_FAN, meshIBOSize, GL_UNSIGNED_INT, nullptr);
  }

  gl->glBindVertexArray(0);

  shaders[settings->currentShader]->release();

  // disable it again as you might want to draw something else at some point
  gl->glDisable(GL_PRIMITIVE_RESTART);
  
  // Draw colored edges if enabled
  if (settings->showSharpEdges && edgeVertexCount > 0 && edgeShader) {
    // Use edge shader for colored edge rendering
    edgeShader->bind();
    
    // Set up uniforms for edge shader
    GLint uniModelView = edgeShader->uniformLocation("modelviewmatrix");
    GLint uniProjection = edgeShader->uniformLocation("projectionmatrix");
    
    if (uniModelView >= 0) {
      gl->glUniformMatrix4fv(uniModelView, 1, false,
                             settings->modelViewMatrix.data());
    }
    if (uniProjection >= 0) {
      gl->glUniformMatrix4fv(uniProjection, 1, false,
                             settings->projectionMatrix.data());
    }
    
    gl->glBindVertexArray(edgeVAO);
    gl->glLineWidth(2.5f);  // Make edges slightly thicker for visibility
    gl->glDrawArrays(GL_LINES, 0, edgeVertexCount);
    gl->glLineWidth(1.0f);  // Reset line width
    gl->glBindVertexArray(0);
    
    edgeShader->release();
  }
  
  // Draw colored vertices if enabled
  if (settings->showVertices && vertexDisplayCount > 0 && edgeShader) {
    // Use edge shader for colored vertex rendering (same shader works for points)
    edgeShader->bind();
    
    // Set up uniforms for vertex shader
    GLint uniModelView = edgeShader->uniformLocation("modelviewmatrix");
    GLint uniProjection = edgeShader->uniformLocation("projectionmatrix");
    
    if (uniModelView >= 0) {
      gl->glUniformMatrix4fv(uniModelView, 1, false,
                             settings->modelViewMatrix.data());
    }
    if (uniProjection >= 0) {
      gl->glUniformMatrix4fv(uniProjection, 1, false,
                             settings->projectionMatrix.data());
    }
    
    gl->glBindVertexArray(vertexVAO);
    gl->glPointSize(6.0f);  // Make vertices visible
    gl->glDrawArrays(GL_POINTS, 0, vertexDisplayCount);
    gl->glPointSize(1.0f);  // Reset point size
    gl->glBindVertexArray(0);
    
    edgeShader->release();
  }
}
