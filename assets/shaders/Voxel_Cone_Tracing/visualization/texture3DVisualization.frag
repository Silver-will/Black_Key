#version 430


layout(location = 0) in vec4 inColor;

layout (location = 0) out vec4 out_color;

const float EPSILON = 0.00001;


void main() 
{
	out_color = inColor;
	out_color.a = 1.0;
}
