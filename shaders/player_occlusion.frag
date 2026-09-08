uniform sampler2D texture;
uniform sampler2D wallDepth;
uniform vec2 renderSize;
uniform float footDepth;
uniform vec2 texelSize;
uniform bool outlined;
uniform vec4 outlineColor;
uniform float thickness;

void main() {
    vec2 uv = gl_TexCoord[0].xy;
    vec4 base = texture2D(texture, uv) * gl_Color;
    vec4 wall = texture2D(wallDepth, gl_FragCoord.xy / renderSize);
    float depth = floor(wall.r * 255.0 + 0.5) * 256.0 + floor(wall.g * 255.0 + 0.5) - 32768.0;
    bool hidden = wall.a > 0.5 && footDepth < depth - 1.0;
    if (base.a > 0.01) {
        // Black, slightly translucent: wall details remain visible through the player.
        gl_FragColor = hidden ? vec4(0.015, 0.02, 0.025, base.a * 0.62) : base;
        return;
    }
    if (outlined && !hidden) {
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                if (texture2D(texture, uv + vec2(float(x),float(y)) * texelSize * thickness).a > 0.01) {
                    gl_FragColor = outlineColor;
                    return;
                }
            }
        }
    }
    gl_FragColor = vec4(0.0);
}
