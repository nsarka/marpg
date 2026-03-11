uniform sampler2D texture;
uniform vec2 texelSize;      // 1.0 / texture size
uniform vec4 outlineColor;   // rgba in 0..1
uniform float thickness;     // in texels

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 base = texture2D(texture, uv);

    // Keep original sprite pixels
    if (base.a > 0.0)
    {
        gl_FragColor = gl_Color * base;
        return;
    }

    float r = thickness;

    // 8-neighbor check; you can expand this if you want a rounder outline
    vec2 offsets[8];
    offsets[0] = vec2(-r,  0.0) * texelSize;
    offsets[1] = vec2( r,  0.0) * texelSize;
    offsets[2] = vec2(0.0, -r) * texelSize;
    offsets[3] = vec2(0.0,  r) * texelSize;
    offsets[4] = vec2(-r, -r) * texelSize;
    offsets[5] = vec2(-r,  r) * texelSize;
    offsets[6] = vec2( r, -r) * texelSize;
    offsets[7] = vec2( r,  r) * texelSize;

    for (int i = 0; i < 8; ++i)
    {
        vec4 s = texture2D(texture, uv + offsets[i]);
        if (s.a > 0.0)
        {
            gl_FragColor = outlineColor;
            return;
        }
    }

    gl_FragColor = vec4(0.0);
}