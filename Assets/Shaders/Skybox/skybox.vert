#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 matProjection;
uniform mat4 matView;

out vec3 fragTexCoord;

void main() {
    fragTexCoord = vertexPosition;
    
    mat4 rotView = mat4(mat3(matView));
    gl_Position = matProjection * rotView * vec4(vertexPosition, 1.0);
}