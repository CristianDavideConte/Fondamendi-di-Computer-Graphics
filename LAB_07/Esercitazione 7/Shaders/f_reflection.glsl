#version 450 core

out vec4 FragColor;

in vec3 fragPos;
in vec3 fragNormal;

//compute the reflection in world space, using a camera world position uniform
uniform vec3 camera_position;
uniform samplerCube skybox;

void main()
{ 
	vec3 E = normalize(fragPos - camera_position);
	vec3 R = reflect(E, normalize(fragNormal));
	FragColor = texture(skybox, R);
}