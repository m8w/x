#pragma once
// Platform-portable OpenGL header.
// On macOS: use the system OpenGL/gl3.h (OpenGL 4.1 core, no GLEW needed).
// On Linux/Windows: use GLEW which loads all extensions at runtime.
#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#else
#  include <GL/glew.h>
#endif
