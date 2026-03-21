#pragma once
#include "gl_includes.h"
#include <string>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    // Alias used by MilkDropGLRenderer
    bool load(const std::string& vertPath, const std::string& fragPath) {
        return loadFromFiles(vertPath, fragPath);
    }
    void use() const;
    GLuint id() const { return m_id; }

    void setFloat (const char* name, float v) const;
    void setFloat2(const char* name, float x, float y) const;
    void setFloat3(const char* name, float x, float y, float z) const;
    void setFloat4(const char* name, float x, float y, float z, float w) const;
    void setInt   (const char* name, int v)   const;
    void setBool  (const char* name, bool v)  const;

    // Convenience aliases matching MilkDropGLRenderer call sites
    void setVec2(const char* n, float x, float y) const { setFloat2(n, x, y); }
    void setVec4(const char* n, float x, float y, float z, float w) const {
        setFloat4(n, x, y, z, w);
    }

private:
    GLuint m_id = 0;
    static GLuint compile(GLenum type, const std::string& src);
    static std::string readFile(const std::string& path);
};
