#version 450

layout(push_constant) uniform Push {
    vec4 rect;
    vec4 params;
    vec2 screen;
    float mode;
    float opacity;
} pc;

layout(set = 0, binding = 0) uniform sampler2D windowTexture;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;   // premultiplied alpha

void main() {
    if (pc.mode > 1.5) {
        // Soft drop shadow: darkness falls off with the distance from the
        // window rectangle, across a margin as wide as params.x.
        vec2 p = uv * pc.rect.zw;
        vec2 lo = pc.params.xy;
        vec2 hi = pc.params.xy + pc.params.zw;
        float dist = length(max(max(lo - p, p - hi), vec2(0.0)));
        float a = 1.0 - clamp(dist / pc.params.x, 0.0, 1.0);
        color = vec4(0.0, 0.0, 0.0, a * a * pc.opacity);
        return;
    }
    vec4 texel = texture(windowTexture, uv);
    if (pc.mode < 0.5) {
        texel.a = 1.0;   // depth-24 windows carry garbage in the alpha byte
    }
    color = texel * pc.opacity;
}
