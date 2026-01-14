#version 410
// Tessellation Control Shader (TCS)
layout(vertices = 16) out;

layout(location = 0) in vec3[] vertcoords_vs;
layout(location = 0) out vec3[] vertcoords_tc;

uniform float outerTessLevel = 4.0;
uniform float innerTessLevel = 4.0;

void main() {
  vertcoords_tc[gl_InvocationID] = vertcoords_vs[gl_InvocationID];

  if (gl_InvocationID == 0) {
    gl_TessLevelOuter[0] = outerTessLevel;
    gl_TessLevelOuter[1] = outerTessLevel;
    gl_TessLevelOuter[2] = outerTessLevel;
    gl_TessLevelOuter[3] = outerTessLevel;

    gl_TessLevelInner[0] = innerTessLevel;
    gl_TessLevelInner[1] = innerTessLevel;
  }
}
