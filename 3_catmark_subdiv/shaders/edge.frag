#version 410
// Edge fragment shader - just output the vertex color

layout(location = 0) in vec3 vertcolor_fs;

out vec4 fColor;

void main() {
  fColor = vec4(vertcolor_fs, 1.0);
}
