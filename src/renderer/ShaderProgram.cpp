#include "ShaderProgram.h"
#include <fstream>
#include <sstream>
#include <cstdio>

ShaderProgram::~ShaderProgram() {
    if (m_id) glDeleteProgram(m_id);
}

std::string ShaderProgram::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "Shader: cannot open %s\n", path.c_str()); return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint ShaderProgram::compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool ShaderProgram::loadFromFiles(const std::string& vertPath,
                                   const std::string& fragPath) {
    GLuint vs = compile(GL_VERTEX_SHADER,   readFile(vertPath));
    GLuint fs = compile(GL_FRAGMENT_SHADER, readFile(fragPath));
    if (!vs || !fs) return false;

    if (m_id) glDeleteProgram(m_id);
    m_id = glCreateProgram();
    glAttachShader(m_id, vs);
    glAttachShader(m_id, fs);
    glLinkProgram(m_id);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(m_id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(m_id, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader link error:\n%s\n", log);
        return false;
    }
    return true;
}

void ShaderProgram::use()  const { glUseProgram(m_id); }

void ShaderProgram::setFloat(const char* n, float v) const {
    glUniform1f(glGetUniformLocation(m_id, n), v);
}
void ShaderProgram::setFloat2(const char* n, float x, float y) const {
    glUniform2f(glGetUniformLocation(m_id, n), x, y);
}
void ShaderProgram::setFloat3(const char* n, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(m_id, n), x, y, z);
}
void ShaderProgram::setFloat4(const char* n, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(m_id, n), x, y, z, w);
}
void ShaderProgram::setInt(const char* n, int v) const {
    glUniform1i(glGetUniformLocation(m_id, n), v);
}
void ShaderProgram::setBool(const char* n, bool v) const {
    glUniform1i(glGetUniformLocation(m_id, n), v ? 1 : 0);
}
