#version 450 core

out vec4 FragColor;

in vec3 fragPos;
in vec3 fragNormal;

uniform vec3 camera_position;
uniform samplerCube skybox;

void main()
{ 
	float ratio = 1.00 / 1.52;
	vec3 E = normalize(fragPos - camera_position);
	vec3 R = refract(E, normalize(fragNormal), ratio);
	FragColor = texture(skybox, R);
} 