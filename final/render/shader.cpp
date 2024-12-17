#include "shader.h"

// Utility function to read a file into a string
std::string ReadFile(const char* file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Unable to open shader file: " << file_path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Utility function to compile a shader and check for errors
GLuint CompileShader(GLenum shaderType, const std::string& shaderCode) {
    GLuint shaderID = glCreateShader(shaderType);
    const char* shaderCodeCStr = shaderCode.c_str();
    glShaderSource(shaderID, 1, &shaderCodeCStr, nullptr);
    glCompileShader(shaderID);

    GLint success;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength + 1);
        glGetShaderInfoLog(shaderID, logLength, nullptr, infoLog.data());
        std::cerr << "Error compiling shader: " << infoLog.data() << std::endl;
        return 0;
    }
    return shaderID;
}

// Load shaders from file (vertex, fragment, and optional geometry shader)
GLuint LoadShadersFromFile(const char* vertex_file_path, const char* fragment_file_path, const char* geometry_file_path) {
    std::string vertexCode = ReadFile(vertex_file_path);
    std::string fragmentCode = ReadFile(fragment_file_path);
    std::string geometryCode = geometry_file_path ? ReadFile(geometry_file_path) : "";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode);
    GLuint geometryShader = 0;

    // If geometry shader path is provided, compile the geometry shader
    if (!geometryCode.empty()) {
        geometryShader = CompileShader(GL_GEOMETRY_SHADER, geometryCode);
    }

    // Create shader program
    GLuint programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    if (geometryShader) {
        glAttachShader(programID, geometryShader);
    }

    // Link the program
    GLint success;
    glLinkProgram(programID);
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength + 1);
        glGetProgramInfoLog(programID, logLength, nullptr, infoLog.data());
        std::cerr << "Error linking shader program: " << infoLog.data() << std::endl;
        return 0;
    }

    // Clean up shaders as they are now linked into the program
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader) {
        glDeleteShader(geometryShader);
    }

    return programID;
}

// Load shaders from string (vertex, fragment, and optional geometry shader)
GLuint LoadShadersFromString(std::string VertexShaderCode, std::string FragmentShaderCode, std::string GeometryShaderCode) {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VertexShaderCode);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderCode);
    GLuint geometryShader = 0;

    // If geometry shader code is provided, compile the geometry shader
    if (!GeometryShaderCode.empty()) {
        geometryShader = CompileShader(GL_GEOMETRY_SHADER, GeometryShaderCode);
    }

    // Create shader program
    GLuint programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    if (geometryShader) {
        glAttachShader(programID, geometryShader);
    }

    // Link the program
    GLint success;
    glLinkProgram(programID);
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength + 1);
        glGetProgramInfoLog(programID, logLength, nullptr, infoLog.data());
        std::cerr << "Error linking shader program: " << infoLog.data() << std::endl;
        return 0;
    }

    // Clean up shaders as they are now linked into the program
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader) {
        glDeleteShader(geometryShader);
    }

    return programID;
}
