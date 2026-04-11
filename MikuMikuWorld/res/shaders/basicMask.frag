#version 330 core

precision mediump float;

in vec2 baseUV;
in vec2 maskUV;
in vec4 color;

out vec4 fragColor;

uniform sampler2D baseTex;
uniform sampler2D maskTex;

void main()
{
    vec4 baseColor = texture(baseTex, baseUV) * color;
    vec4 mask = texture(maskTex, maskUV);

    fragColor = baseColor * mask;
}