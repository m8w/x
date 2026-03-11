#pragma once
// Platform-portable OpenGL header.
// On macOS: use the system OpenGL/gl3.h (OpenGL 4.1 core, no GLEW needed).
// On Linux/Windows: use GLEW which loads all extensions at runtime.
#ifdef __APPLE__
#  ifndef GL_SILENCE_DEPRECATION
#    define GL_SILENCE_DEPRECATION
#  endif
#  include <OpenGL/gl3.h>
#else
#  include <GL/glew.h>
#endif
