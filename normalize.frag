#version 330 core
uniform sampler2D accumColTex;   // RGBA32F
uniform sampler2D accumWTex;     // R32F
in vec2 vUV;
layout(location = 0) out vec4 Frag;

void main()
{
    vec4  sum = texture(accumColTex, vUV);
    float w   = texture(accumWTex , vUV).r;

    if(w < 1e-6)
        Frag = vec4(1.0, 0.0, 0.0, 1.0);   // 没人投射 → 输出黑
    else
        Frag = vec4(sum.rgb / w, 1.0);
}