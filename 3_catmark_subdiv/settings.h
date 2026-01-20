#ifndef SETTINGS_H
#define SETTINGS_H

#include <QMatrix4x4>

#include "mesh/halfedge.h"
#include "shadertypes.h"

/**
 * Struct that contains all the settings of the program. Initialised with a
 * number of default values.
 */
typedef struct Settings {
  bool modelLoaded = false;
  bool wireframeMode = true;
  bool tesselationMode = false;
  bool showCpuMesh = true;

  // Show limit positions on or off
  bool showLimitPosition = false;

  // Show sharp edges with colors (red = sharp, yellow = smooth)
  bool showSharpEdges = true;

  // Show vertices with colors (different color for boundary vertices)
  bool showVertices = false;

  // Fixed mode: uniform cubic B-spline (no toggle)
  bool useBezierPatch = false;

  float FoV = 80;
  float dispRatio = 16.0f / 9.0f;
  float rotAngle = 0.0f;

  int subdivisionLevel = 0;

  Vertex* selectedVertex;
  HalfEdge* selectedEdge;

  bool uniformUpdateRequired = true;

  ShaderType currentShader = ShaderType::PHONG;

  QMatrix4x4 modelViewMatrix, projectionMatrix;
  QMatrix3x3 normalMatrix;
} Settings;

#endif  // SETTINGS_H
