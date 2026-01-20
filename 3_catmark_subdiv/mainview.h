#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QMouseEvent>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QQuaternion>

#include "mesh/mesh.h"
#include "renderers/meshrenderer.h"
#include "renderers/tessrenderer.h"

/**
 * @brief The MainView class represents the main view of the UI. It handles and
 * orchestrates the different renderers.
 */
class MainView : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
  Q_OBJECT

 public:
  MainView(QWidget* Parent = nullptr);
  ~MainView() override;

  void updateMatrices();
  void updateUniforms();
  void updateBuffers(Mesh& currentMesh);
  void updateSharpness(int sharpness);

  // Edge selection
  void setCurrentMesh(Mesh* mesh) { currentMesh = mesh; }
  int getSelectedEdgeSharpness() const { return selectedEdgeSharpness; }
  void clearEdgeSelection();
  
  // Vertex selection
  int getSelectedVertexSharpEdgeCount() const { return selectedVertexSharpEdgeCount; }
  void clearVertexSelection();

 signals:
  void edgeSelected(int sharpness);  // Signal emitted when an edge is selected
  void vertexSelected(int sharpEdgeCount);  // Signal emitted when a vertex is selected

 protected:
  void initializeGL() override;
  void resizeGL(int newWidth, int newHeight) override;
  void paintGL() override;

  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  QVector2D toNormalizedScreenCoordinates(float x, float y);
  HalfEdge* pickEdgeAtScreenPosition(float x, float y);  // Find edge closest to click position
  Vertex* pickVertexAtScreenPosition(float x, float y);  // Find vertex closest to click position
  int countSharpEdgesAtVertex(const Vertex& vertex) const;  // Count sharp edges incident to vertex

  QOpenGLDebugLogger debugLogger;
  
  Mesh* currentMesh = nullptr;  // Reference to current mesh for edge/vertex picking
  int selectedEdgeSharpness = -1;  // -1 means no selection
  int selectedVertexSharpEdgeCount = -1;  // -1 means no selection

  // for mouse interactions:
  float scale;
  QVector3D oldVec;
  QQuaternion rotationQuaternion;
  bool dragging;

  MeshRenderer meshRenderer;
  TessellationRenderer tessellationRenderer;

  Settings settings;

  // we make mainwindow a friend so it can access settings
  friend class MainWindow;
 private slots:
  void onMessageLogged(QOpenGLDebugMessage Message);
};

#endif  // MAINVIEW_H
