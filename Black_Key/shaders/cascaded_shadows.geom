#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

void main(void)
{
	gl_Layer = 0;
	gl_Position = vec4(3,1,2,0);
	EmitVertex();

	gl_Position = vec4(3,1,2,0);
	EmitVertex();

	gl_Position = vec4(3,1,2,0);
	EmitVertex();
	EndPrimitive();


	/*for (int i = 0; i < 3; ++i)
	{
		gl_Position = vec4(1,1,1,1);
		gl_Layer = 0;
		EmitVertex();
	}
	*/
	EndPrimitive();
}  
