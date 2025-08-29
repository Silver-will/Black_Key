#version 430


layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout (location = 0) out vec4 out_color;

const float EPSILON = 0.00001;


void main() 
{
	out_color = vec4(1,0,0,0);
	
	//if (u_borderWidth > EPSILON)
		//out_color = mix(u_borderColor, In.color, min(min(In.uv.x, min(In.uv.y, min((1.0 - In.uv.x), (1.0 - In.uv.y)))) / u_borderWidth, 1.0));
		
	out_color.a = 1.0;
}
