#include "render_resources.hh"

#include <rlgl.h>

namespace viewer {
namespace {

std::string ShaderUniformKey(Shader shader, const char *name) {
    return std::to_string(shader.id) + ':' + name;
}

}

void CacheShaderUniforms(RenderResources &resources, Shader shader, std::initializer_list<const char *> names) {
    for (const char *name: names)
        resources.uniformLocations.emplace(ShaderUniformKey(shader, name), GetShaderLocation(shader, name));
}

int RenderResources::Uniform(Shader shader, const char *name) const {
    auto found = uniformLocations.find(ShaderUniformKey(shader, name));
    return found == uniformLocations.end() ? -1 : found->second;
}

void RenderResources::SetVector3(Shader shader, const char *name, Vector3 value) const {
    int location = Uniform(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
}

void RenderResources::SetFloat(Shader shader, const char *name, float value) const {
    int location = Uniform(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
}

void RenderResources::SetVector4(Shader shader, const char *name, Vector4 value) const {
    int location = Uniform(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC4);
}

void RenderResources::SetMatrix(Shader shader, const char *name, Matrix value) const {
    int location = Uniform(shader, name);
    if (location >= 0) SetShaderValueMatrix(shader, location, value);
}

void RenderResources::SetSourceTexelSize(Shader shader, const Texture2D &texture, float scale) const {
    Vector2 texelSize {scale / static_cast<float>(texture.width), scale / static_cast<float>(texture.height)};
    int location = Uniform(shader, "sourceTexelSize");
    if (location >= 0) SetShaderValue(shader, location, &texelSize, SHADER_UNIFORM_VEC2);
}

void ConfigureStarShader(Shader &shader, bool hasNormal, bool hasTexcoord) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "in_POSITION0");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = hasNormal ? GetShaderLocationAttrib(shader, "in_NORMAL0") : -1;
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = hasTexcoord ? GetShaderLocationAttrib(shader, "in_TEXCOORD0") : -1;
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
}

void ConfigurePlanetThumbShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = -1;
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = -1;
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    // DrawMesh() binds maps[MATERIAL_MAP_CUBEMAP] as a cubemap and writes its slot into
    // locs[SHADER_LOC_MAP_CUBEMAP], which is how `_HeightMap` reaches the fragment program.
    shader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader, "_HeightMap");
}

void ConfigureGalaxyStarPointShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(shader, "vertexColor");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(shader, "colDiffuse");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

void ConfigureGalaxyLineShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = -1;
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = -1;
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

void ConfigureGalaxyGridShader(Shader &shader, bool hasTexcoord) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = hasTexcoord ? GetShaderLocationAttrib(shader, "vertexTexCoord") : -1;
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = -1;
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

void ConfigureScreenShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

}
