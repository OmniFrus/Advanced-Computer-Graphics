#version 410
// Vertex shader

layout(location = 0) in vec3 vertcoords;
layout(location = 0) out vec3 vertcoords_vs;

void main() {
  vertcoords_vs = vertcoords;
  gl_Position = vec4(vertcoords, 1.0);
}
