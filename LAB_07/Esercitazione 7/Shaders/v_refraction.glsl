#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 fragPos;
out vec3 fragNormal;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

void main()
{
	fragPos = vec3(M * vec4(aPos, 1.0)); //in WCS
	fragNormal = mat3(transpose(inverse(M))) * aNormal;

	gl_Position = P * V * M * vec4(aPos, 1.0);
} 