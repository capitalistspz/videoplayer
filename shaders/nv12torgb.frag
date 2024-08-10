#version 450

layout (location = 0) in vec2 pixTexCoord;

layout (binding = 0) uniform sampler2D yTex;
layout (binding = 1) uniform sampler2D uvTex;

out vec4 FragColor;

void main()
{
    float y = texture(yTex, pixTexCoord).r;
    vec2 uvVec = texture(uvTex, pixTexCoord).rg;

    float yA = y - 0.0625;
    float uA = uvVec.r - 0.5;
    float vA = uvVec.g - 0.5;

    float r = 1.164383 * yA + 1.596027 * vA;
    float g = 1.164383 * yA - 0.391762 * uA - 0.812968 * vA;
    float b = 1.164383 * yA + 2.017232 * uA;
    FragColor = vec4(r, g, b, 1.0);
}