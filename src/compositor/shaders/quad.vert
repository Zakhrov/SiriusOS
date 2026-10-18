#version 450

// One screen-space rectangle per draw call, generated from the vertex index.
layout(push_constant) uniform Push {
    vec4 rect;     // x, y, width, height in pixels
    vec4 params;   // shadow: window rectangle inside the quad (x, y, width, height)
    vec2 screen;   // output size in pixels
    float mode;    // 0 opaque texture, 1 premultiplied-alpha texture, 2 shadow
    float opacity;
    vec4 uvRect;   // part of the texture to show: x, y, width, height in 0..1
} pc;

layout(location = 0) out vec2 uv;

void main() {
    const int i = gl_VertexIndex;
    vec2 corner = vec2((i == 1 || i == 2 || i == 4) ? 1.0 : 0.0,
                       (i == 2 || i == 4 || i == 5) ? 1.0 : 0.0);
    uv = pc.uvRect.xy + corner * pc.uvRect.zw;
    vec2 pixel = pc.rect.xy + corner * pc.rect.zw;
    gl_Position = vec4(pixel / pc.screen * 2.0 - 1.0, 0.0, 1.0);
}
