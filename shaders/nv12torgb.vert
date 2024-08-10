#version 450

layout (location = 0) in vec2 inPosCoord;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 pixTexCoord;

void main()
{
    gl_Position = vec4(inPosCoord, 0.0, 1.0);
    pixTexCoord = inTexCoord;
}