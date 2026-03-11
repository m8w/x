#pragma once
#include "gl_includes.h"
#include <string>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    void use() const;
    GLuint id() const { return m_id; }

    void setFloat (const char* name, float v) const;
    void setFloat2(const char* name, float x, float y) const;
    void setInt   (const char* name, int v)   const;
    void setBool  (const char* name, bool v)  const;

private:
    GLuint m_id = 0;
    static GLuint compile(GLenum type, const std::string& src);
    static std::string readFile(const std::string& path);
};
