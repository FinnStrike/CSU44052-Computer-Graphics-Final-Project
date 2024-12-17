#ifndef _SHADER_H_
#define _SHADER_H_

#include "headers.h"

GLuint LoadShadersFromFile(const char *vertex_file_path, const char *fragment_file_path, const char* geometry_file_path = nullptr);

GLuint LoadShadersFromString(std::string VertexShaderCode, std::string FragmentShaderCode, std::string GeometryShaderCode = "");

#endif
