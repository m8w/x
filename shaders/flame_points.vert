#version 410 core
// Trivial passthrough — one point per chaos-game walk. All real work lives
// in flame_points.geom, which needs gl_VertexID to seed its hash.
flat out int v_id;

void main() {
    v_id = gl_VertexID;
    gl_Position = vec4(0.0);   // unused; the geometry shader sets real positions
}
