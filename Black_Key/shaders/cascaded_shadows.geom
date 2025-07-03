#version 460 core

#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 skyMat;
	vec4 cameraPos;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec3 cascadeConfigData;
	uint lightCount;
	vec4 distances;
	mat4 lightMatrices[8];
	float cascadeDistances[8];
} sceneData;


layout (triangles, invocations = 5) in;
layout (triangle_strip, max_vertices = 3) out;

invariant gl_Position;
void main(void)
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = sceneData.lightMatrices[gl_InvocationID] * gl_in[i].gl_Position.xyzw;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}  
