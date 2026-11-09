#version 410 core
// Trivial passthrough — one point per sample. All real work (sampling a
// random c, iterating the orbit, deciding what to emit) happens in
// buddhabrot_points.geom, which needs gl_VertexID to seed its hash.
flat out int v_id;

void main() {
    v_id = gl_VertexID;
    gl_Position = vec4(0.0);   // unused; the geometry shader sets real positions
}
