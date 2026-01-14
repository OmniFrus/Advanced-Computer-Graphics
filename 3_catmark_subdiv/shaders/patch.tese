#version 410
layout(quads, fractional_even_spacing, ccw) in;

layout(location = 0) in vec3[] vertcoords_tc;
layout(location = 0) out vec3 vertcoords_te;
layout(location = 1) out vec3 vertnormals_te;

uniform mat4 modelviewmatrix;
uniform mat4 projectionmatrix;
uniform mat3 normalmatrix;
uniform bool useBezierPatch = false;

vec4 bernstein(float t) {
  float it = 1.0 - t;
  return vec4(it * it * it, 3.0 * it * it * t, 3.0 * it * t * t, t * t * t);
}

vec4 dBernstein(float t) {
  float it = 1.0 - t;
  return vec4(-3.0 * it * it, 3.0 * it * it - 6.0 * it * t, 6.0 * it * t - 3.0 * t * t,
              3.0 * t * t);
}

vec4 bspline(float t) {
  float t2 = t * t;
  float t3 = t2 * t;
  return vec4((1.0 - 3.0 * t + 3.0 * t2 - t3) / 6.0,
              (4.0 - 6.0 * t2 + 3.0 * t3) / 6.0,
              (1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3) / 6.0,
              t3 / 6.0);
}

vec4 dBspline(float t) {
  float t2 = t * t;
  return vec4((-3.0 + 6.0 * t - 3.0 * t2) / 6.0,
              (-12.0 * t + 9.0 * t2) / 6.0,
              (3.0 + 6.0 * t - 9.0 * t2) / 6.0,
              (3.0 * t2) / 6.0);
}

void main() {
  float u = gl_TessCoord.x;
  float v = gl_TessCoord.y;

vec4 Bu = useBezierPatch ? bernstein(u) : bspline(u);
vec4 Bv = useBezierPatch ? bernstein(v) : bspline(v);
vec4 dBu = useBezierPatch ? dBernstein(u) : dBspline(u);
vec4 dBv = useBezierPatch ? dBernstein(v) : dBspline(v);

  vec3 p = vec3(0.0);
  vec3 du = vec3(0.0);
  vec3 dv = vec3(0.0);

  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int idx = r * 4 + c;
      vec3 cp = vertcoords_tc[idx];
      float br = Bv[r];
      float bc = Bu[c];
      float dbr = dBv[r];
      float dbc = dBu[c];
      p += br * bc * cp;
      du += br * dbc * cp;
      dv += dbr * bc * cp;
    }
  }

  vec3 normal = normalize(normalmatrix * normalize(cross(du, dv)));

  gl_Position = projectionmatrix * modelviewmatrix * vec4(p, 1.0);
  vertcoords_te = vec3(modelviewmatrix * vec4(p, 1.0));
  vertnormals_te = normal;
}
