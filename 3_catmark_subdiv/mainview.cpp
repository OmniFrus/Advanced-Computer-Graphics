#include "mainview.h"

#include <math.h>
#include <algorithm>
#include <limits>

#include <QLoggingCategory>
#include <QOpenGLVersionFunctionsFactory>
#include <QSet>

#include "mesh/mesh.h"
#include "mesh/halfedge.h"
#include "mesh/vertex.h"
#include <QSet>
extern bool g_showLimitPosition;

/**
 * @brief MainView::MainView
 * @param Parent
 */
MainView::MainView(QWidget* Parent) : QOpenGLWidget(Parent), scale(1.0f) {}

/**
 * @brief MainView::~MainView Deconstructs the main view.
 */
MainView::~MainView() {
  debugLogger.stopLogging();
  makeCurrent();
}

/**
 * @brief MainView::initializeGL Initializes the opengl functions and settings,
 * initialises the renderers and sets up the debugger.
 */
void MainView::initializeGL() {
  initializeOpenGLFunctions();
  qDebug() << ":: OpenGL initialized";

  connect(&debugLogger, SIGNAL(messageLogged(QOpenGLDebugMessage)), this,
          SLOT(onMessageLogged(QOpenGLDebugMessage)), Qt::DirectConnection);

  if (debugLogger.initialize()) {
    QLoggingCategory::setFilterRules(
        "qt.*=false\n"
        "qt.text.font.*=false");
    qDebug() << ":: Logging initialized";
    debugLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
    debugLogger.enableMessages();
  }

  QString glVersion;
  glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  qDebug() << ":: Using OpenGL" << qPrintable(glVersion);

  makeCurrent();
  // Enable depth buffer
  glEnable(GL_DEPTH_TEST);
  // Default is GL_LESS
  glDepthFunc(GL_LEQUAL);

  // grab the opengl context
  QOpenGLFunctions_4_1_Core* functions =
      QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_1_Core>(
          this->context());

  // initialize renderers here with the current context
  meshRenderer.init(functions, &settings);
  tessellationRenderer.init(functions, &settings);
  updateMatrices();
}

/**
 * @brief MainView::resizeGL Handles window resizing.
 * @param newWidth The new width of the window in pixels.
 * @param newHeight The new height of the window in pixels.
 */
void MainView::resizeGL(int newWidth, int newHeight) {
  qDebug() << ".. resizeGL";

  settings.dispRatio = float(newWidth) / float(newHeight);

  settings.projectionMatrix.setToIdentity();
  settings.projectionMatrix.perspective(settings.FoV, settings.dispRatio, 0.1f,
                                        40.0f);
  updateMatrices();
}

/**
 * @brief MainView::updateMatrices Updates the matrices used for the model
 * transforms.
 */
void MainView::updateMatrices() {
  settings.modelViewMatrix.setToIdentity();
  settings.modelViewMatrix.translate(QVector3D(0.0, 0.0, -3.0));
  settings.modelViewMatrix.scale(scale);
  settings.modelViewMatrix.rotate(rotationQuaternion);

  settings.normalMatrix = settings.modelViewMatrix.normalMatrix();

  settings.uniformUpdateRequired = true;

  update();
}

/**
 * @brief MainView::updateBuffers Updates the buffers of the renderers.
 * @param mesh The mesh used to update the buffer content with.
 */
void MainView::updateBuffers(Mesh& mesh) {
  g_showLimitPosition = settings.showLimitPosition;
  mesh.extractAttributes(settings.selectedEdge, settings.selectedVertex);
  meshRenderer.updateBuffers(mesh);
  tessellationRenderer.updateBuffers(mesh);
  currentMesh = &mesh;  // Store reference for edge picking
  update();
}

void MainView::updateSharpness(float sharpness) {
    if (settings.selectedEdge != nullptr) {
      settings.selectedEdge->sharpness = sharpness;
      if (settings.selectedEdge->twin != nullptr) {
        settings.selectedEdge->twin->sharpness = sharpness;
      }
      if (currentMesh != nullptr) {
        updateBuffers(*currentMesh);
      }
    }
}

void MainView::clearEdgeSelection() {
  settings.selectedEdge = nullptr;  // Clear the selected edge reference
  selectedEdgeSharpness = -2.0f;
  emit edgeSelected(-2.0f);
  if (currentMesh != nullptr) {
    updateBuffers(*currentMesh);  // Update buffers to remove highlighting
  } else {
    update();
  }
}

void MainView::clearVertexSelection() {
  settings.selectedVertex = nullptr;  // Clear the selected vertex reference
  selectedVertexSharpEdgeCount = -1;
  emit vertexSelected(-1);
  if (currentMesh != nullptr) {
    updateBuffers(*currentMesh);  // Update buffers to remove highlighting
  } else {
    update();
  }
}

/**
 * @brief MainView::paintGL Draw call.
 */
void MainView::paintGL() {
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (settings.wireframeMode) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  if (settings.modelLoaded) {
    if (settings.showCpuMesh) {
      meshRenderer.draw();
    }
    if (settings.tesselationMode) {
      tessellationRenderer.draw();
    }

    if (settings.uniformUpdateRequired) {
      settings.uniformUpdateRequired = false;
    }
  }
}

/**
 * @brief MainView::toNormalizedScreenCoordinates Normalizes the mouse
 * coordinates to screen coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return A vector containing the normalized x and y screen coordinates.
 */
QVector2D MainView::toNormalizedScreenCoordinates(float x, float y) {
  float xRatio = x / float(width());
  float yRatio = y / float(height());

  // By default, the drawing canvas is the square [-1,1]^2:
  float xScene = (1 - xRatio) * -1 + xRatio * 1;
  // Note that the origin of the canvas is in the top left corner (not the lower
  // left).
  float yScene = yRatio * -1 + (1 - yRatio) * 1;

  return {xScene, yScene};
}

/**
 * @brief MainView::mouseMoveEvent Handles the dragging and rotating of the mesh
 * by looking at the mouse movement.
 * @param event Mouse event.
 */
void MainView::mouseMoveEvent(QMouseEvent* event) {
  if (event->buttons() == Qt::LeftButton) {
    QVector2D sPos = toNormalizedScreenCoordinates(event->position().x(),
                                                   event->position().y());
    QVector3D newVec = QVector3D(sPos.x(), sPos.y(), 0.0);

    // project onto sphere
    float sqrZ = 1.0f - QVector3D::dotProduct(newVec, newVec);
    if (sqrZ > 0) {
      newVec.setZ(sqrt(sqrZ));
    } else {
      newVec.normalize();
    }

    QVector3D v2 = newVec.normalized();
    // reset if we are starting a drag
    if (!dragging) {
      dragging = true;
      oldVec = newVec;
      return;
    }

    // calculate axis and angle
    QVector3D v1 = oldVec.normalized();
    QVector3D N = QVector3D::crossProduct(v1, v2).normalized();
    if (N.length() == 0.0f) {
      oldVec = newVec;
      return;
    }
    float angle = 180.0f / M_PI * acos(QVector3D::dotProduct(v1, v2));
    rotationQuaternion =
        QQuaternion::fromAxisAndAngle(N, angle) * rotationQuaternion;
    updateMatrices();

    // for next iteration
    oldVec = newVec;
  } else {
    // to reset drag
    dragging = false;
    oldVec = QVector3D();
  }
}

/**
 * @brief MainView::mousePressEvent Handles presses by the mouse.
 * Right-click or Shift+Left-click selects edges to show their sharpness.
 * Ctrl+Left-click selects vertices to show their sharp edge count.
 * @param event Mouse event.
 */
void MainView::mousePressEvent(QMouseEvent* event) {
  setFocus();
  
  if (currentMesh == nullptr || !settings.modelLoaded) return;
  
  float x = event->position().x();
  float y = event->position().y();
  
  // Ctrl+Left-click for vertex selection
  bool isVertexSelection = (event->button() == Qt::LeftButton && 
                           event->modifiers() & Qt::ControlModifier);
  
  // Right-click or Shift+Left-click for edge selection
  bool isEdgeSelection = (event->button() == Qt::RightButton) ||
                          (event->button() == Qt::LeftButton && 
                           event->modifiers() & Qt::ShiftModifier);
  
  if (isVertexSelection) {
    Vertex* selectedVertex = pickVertexAtScreenPosition(x, y);
    if (selectedVertex != nullptr) {
      // Check if vertex is on boundary - use -2 as sentinel value for "boundary"
      settings.selectedVertex = selectedVertex;
      if (selectedVertex->isBoundaryVertex()) {
        selectedVertexSharpEdgeCount = -2;  // -2 indicates boundary vertex
        emit vertexSelected(-999);
      } else {
        int sharpEdgeCount = countSharpEdgesAtVertex(*selectedVertex);
        selectedVertexSharpEdgeCount = sharpEdgeCount;
        emit vertexSelected(sharpEdgeCount);
      }
      clearEdgeSelection();  // Clear edge selection when selecting vertex
      updateBuffers(*currentMesh);
    } else {
      clearVertexSelection();
    }
  } else if (isEdgeSelection) {
    HalfEdge* selectedEdge = pickEdgeAtScreenPosition(x, y);
    settings.selectedEdge = selectedEdge;
    if (selectedEdge != nullptr) {
      selectedEdgeSharpness = selectedEdge->sharpness;
      emit edgeSelected(selectedEdgeSharpness);
      clearVertexSelection();  // Clear vertex selection when selecting edge
      //currentMesh->getEdgeColors()[0] = QVector3D(0.0f, 0.4f, 0.0f);
      updateBuffers(*currentMesh);
    } else {
      clearEdgeSelection();
    }
  }
}

/**
 * @brief MainView::wheelEvent Handles zooming of the view.
 * @param event Mouse event.
 */
void MainView::wheelEvent(QWheelEvent* event) {
  // Delta is usually 120
  float phi = 1.0f + (event->angleDelta().y() / 2000.0f);
  scale = fmin(fmax(phi * scale, 0.01f), 100.0f);
  updateMatrices();
}

/**
 * @brief MainView::keyPressEvent Handles keyboard shortcuts. Currently support
 * 'Z' for wireframe mode and 'R' to reset orientation.
 * @param event Mouse event.
 */
void MainView::keyPressEvent(QKeyEvent* event) {
  switch (event->key()) {
    case 'Z':
      settings.wireframeMode = !settings.wireframeMode;
      update();
      break;
    case 'R':
      scale = 1.0f;
      rotationQuaternion = QQuaternion();
      updateMatrices();
      update();
      break;
  }
}

/**
 * @brief MainView::onMessageLogged Helper function for logging messages.
 * @param message The message to log.
 */
void MainView::onMessageLogged(QOpenGLDebugMessage Message) {
  qDebug() << " → Log:" << Message;
}

/**
 * @brief MainView::pickEdgeAtScreenPosition Finds the edge closest to the given
 * screen coordinates by projecting edges to screen space and finding the minimum distance.
 * @param x Screen x coordinate in pixels.
 * @param y Screen y coordinate in pixels.
 * @return Pointer to the closest half-edge, or nullptr if no edge found.
 */
HalfEdge* MainView::pickEdgeAtScreenPosition(float x, float y) {
  if (currentMesh == nullptr) return nullptr;
  
  QVector2D screenPos(x, y);
  QVector<HalfEdge>& halfEdges = currentMesh->getHalfEdges();
  
  float minDistance = std::numeric_limits<float>::max();
  HalfEdge* closestEdge = nullptr;
  
  // Convert screen coordinates to normalized device coordinates
  float ndcX = (2.0f * x / width()) - 1.0f;
  float ndcY = 1.0f - (2.0f * y / height());  // Flip Y axis
  
  // Project all edges to screen space and find the closest one
  QSet<QPair<int, int>> processedEdges;  // Avoid processing same edge twice
  
  for (int h = 0; h < halfEdges.size(); ++h) {
    HalfEdge* edge = &halfEdges[h];
    int v1Idx = edge->origin->index;
    int v2Idx = edge->next->origin->index;
    
    // Create canonical edge representation
    QPair<int, int> edgePair = (v1Idx < v2Idx) ? 
                               QPair<int, int>(v1Idx, v2Idx) : 
                               QPair<int, int>(v2Idx, v1Idx);
    
    if (processedEdges.contains(edgePair)) continue;
    processedEdges.insert(edgePair);
    
    // Get vertex positions in world space
    QVector3D v1World = currentMesh->getVertices()[v1Idx].coords;
    QVector3D v2World = currentMesh->getVertices()[v2Idx].coords;
    
    // Transform to view space
    QVector4D v1View = settings.modelViewMatrix * QVector4D(v1World, 1.0f);
    QVector4D v2View = settings.modelViewMatrix * QVector4D(v2World, 1.0f);
    
    // Project to clip space
    QVector4D v1Clip = settings.projectionMatrix * v1View;
    QVector4D v2Clip = settings.projectionMatrix * v2View;
    
    // Perspective divide to get NDC
    if (v1Clip.w() != 0.0f && v2Clip.w() != 0.0f) {
      QVector3D v1NDC = QVector3D(v1Clip.x(), v1Clip.y(), v1Clip.z()) / v1Clip.w();
      QVector3D v2NDC = QVector3D(v2Clip.x(), v2Clip.y(), v2Clip.z()) / v2Clip.w();
      
      // Check if edge is in front of camera
      if (v1NDC.z() > -1.0f && v1NDC.z() < 1.0f &&
          v2NDC.z() > -1.0f && v2NDC.z() < 1.0f) {
        
        // Calculate distance from click point to edge in screen space
        QVector2D p1(v1NDC.x(), v1NDC.y());
        QVector2D p2(v2NDC.x(), v2NDC.y());
        QVector2D clickPos(ndcX, ndcY);
        
        // Vector from p1 to p2
        QVector2D edgeDir = p2 - p1;
        float edgeLength = edgeDir.length();
        
        if (edgeLength > 0.001f) {
          edgeDir.normalize();
          
          // Vector from p1 to click point
          QVector2D toClick = clickPos - p1;
          
          // Project toClick onto edgeDir
          float projection = QVector2D::dotProduct(toClick, edgeDir);
          
          // Clamp to edge segment
          projection = std::max(0.0f, std::min(projection, edgeLength));
          
          // Find closest point on edge
          QVector2D closestPoint = p1 + edgeDir * projection;
          
          // Distance from click to closest point on edge
          float distance = (clickPos - closestPoint).length();
          
          // Use a threshold (edges closer than this are considered "clicked")
          const float pickThreshold = 0.05f;  // Adjust based on screen size
          
          if (distance < pickThreshold && distance < minDistance) {
            minDistance = distance;
            closestEdge = edge;
          }
        }
      }
    }
  }
  
  return closestEdge;
}

/**
 * @brief MainView::pickVertexAtScreenPosition Finds the vertex closest to the given
 * screen coordinates by projecting vertices to screen space and finding the minimum distance.
 * @param x Screen x coordinate in pixels.
 * @param y Screen y coordinate in pixels.
 * @return Pointer to the closest vertex, or nullptr if no vertex found.
 */
Vertex* MainView::pickVertexAtScreenPosition(float x, float y) {
  if (currentMesh == nullptr) return nullptr;
  
  QVector<Vertex>& vertices = currentMesh->getVertices();
  
  float minDistance = std::numeric_limits<float>::max();
  Vertex* closestVertex = nullptr;
  
  // Convert screen coordinates to normalized device coordinates
  float ndcX = (2.0f * x / width()) - 1.0f;
  float ndcY = 1.0f - (2.0f * y / height());  // Flip Y axis
  
  // Project all vertices to screen space and find the closest one
  for (int v = 0; v < vertices.size(); ++v) {
    Vertex* vertex = &vertices[v];
    
    // Get vertex position in world space
    QVector3D vWorld = vertex->coords;
    
    // Transform to view space
    QVector4D vView = settings.modelViewMatrix * QVector4D(vWorld, 1.0f);
    
    // Project to clip space
    QVector4D vClip = settings.projectionMatrix * vView;
    
    // Perspective divide to get NDC
    if (vClip.w() != 0.0f) {
      QVector3D vNDC = QVector3D(vClip.x(), vClip.y(), vClip.z()) / vClip.w();
      
      // Check if vertex is in front of camera
      if (vNDC.z() > -1.0f && vNDC.z() < 1.0f) {
        
        // Calculate distance from click point to vertex in screen space
        QVector2D vertexPos(vNDC.x(), vNDC.y());
        QVector2D clickPos(ndcX, ndcY);
        
        float distance = (clickPos - vertexPos).length();
        
        // Use a threshold (vertices closer than this are considered "clicked")
        const float pickThreshold = 0.05f;  // Adjust based on screen size
        
        if (distance < pickThreshold && distance < minDistance) {
          minDistance = distance;
          closestVertex = vertex;
        }
      }
    }
  }
  
  return closestVertex;
}

/**
 * @brief MainView::countSharpEdgesAtVertex Counts the number of sharp edges
 * incident to a vertex, similar to CatmullClarkSubdivider::countCreaseEdges.
 * @param vertex The vertex to count sharp edges for.
 * @return The number of unique sharp edges incident to the vertex.
 */
int MainView::countSharpEdgesAtVertex(const Vertex& vertex) const {
  // Count the number of unique sharp edges incident to this vertex
  // Use a more robust approach: iterate through all half-edges and check
  // which ones are incident to this vertex (works for both single and multi-face meshes)
  QSet<int> sharpEdgeIndices;  // Use edge indices to avoid duplicates
  
  if (currentMesh == nullptr) return 0;
  
  // Iterate through all half-edges and check which ones are incident to this vertex
  QVector<HalfEdge>& halfEdges = currentMesh->getHalfEdges();
  for (int h = 0; h < halfEdges.size(); ++h) {
    HalfEdge* edge = &halfEdges[h];
    // Check if this half-edge originates from the vertex
    if (edge->origin == &vertex) {
      if (edge->isSharpEdge()) {
        sharpEdgeIndices.insert(edge->edgeIndex);
      }
    }
  }
  
  return sharpEdgeIndices.size();
}
