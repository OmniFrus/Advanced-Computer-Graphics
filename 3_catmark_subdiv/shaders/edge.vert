#version 410
// Edge vertex shader - simple pass-through with color

layout(location = 0) in vec3 vertcoords_vs;
layout(location = 1) in vec3 vertcolor_vs;

uniform mat4 modelviewmatrix;
uniform mat4 projectionmatrix;

layout(location = 0) out vec3 vertcolor_fs;

void main() {
  gl_Position = projectionmatrix * modelviewmatrix * vec4(vertcoords_vs, 1.0);
  vertcolor_fs = vertcolor_vs;
}
