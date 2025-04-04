#version 330

in vec3 fragTexCoord;

uniform samplerCube environmentMap;

out vec4 finalColor;

void main() {
    finalColor = texture(environmentMap, fragTexCoord);
}