#ifndef GL_EXT_H
#define GL_EXT_H

// Minimal OpenGL function loader for Windows.
// NO macros, NO SDL_opengl.h. Everything explicit.
// Call gl_load_extensions() after SDL_GL_CreateContext().

#include <SDL.h>
#include <stddef.h>

// ── Windows GL types not in our declarations ───────────────────────────────
#ifdef _WIN32
    #include <windows.h>   // for APIENTRY, CALLBACK
#else
    #define APIENTRY
#endif

// ── GL types ───────────────────────────────────────────────────────────────
typedef unsigned int   GLenum;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLboolean;
typedef unsigned char  GLubyte;
typedef float          GLfloat;
typedef void           GLvoid;
typedef char           GLchar;
typedef ptrdiff_t      GLsizeiptr;
typedef unsigned int   GLbitfield;

// ── GL constants ───────────────────────────────────────────────────────────
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NO_ERROR                       0
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_TRIANGLES                      0x0004
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_DEPTH_TEST                     0x0B71
#define GL_LEQUAL                         0x0203
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_VERSION                        0x1F02
#define GL_RENDERER                       0x1F01
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
// Textures
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE0                       0x84C0
#define GL_RGBA                           0x1908
#define GL_RGB                            0x1907
#define GL_TEXTURE_MIN_FILTER             0x2800
#define GL_TEXTURE_MAG_FILTER             0x2801
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_BLEND                          0x0BE2
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_RED                            0x1903
#define GL_LUMINANCE                      0x1909
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_VERTEX_PROGRAM_POINT_SIZE      0x8642  // enables gl_PointSize writes in vertex shader
#define GL_POINT_SPRITE                   0x8861  // enables gl_PointCoord in fragment shader

// ── GL function pointers ───────────────────────────────────────────────────
// Stored as raw pointers. No macros. Use directly via e.g. pgl_glDrawElements(...).

// State
extern "C" {
typedef void   (APIENTRY *PFN_glClear)(GLbitfield mask);
typedef void   (APIENTRY *PFN_glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
typedef void   (APIENTRY *PFN_glEnable)(GLenum cap);
typedef void   (APIENTRY *PFN_glDisable)(GLenum cap);
typedef void   (APIENTRY *PFN_glDepthFunc)(GLenum func);
typedef void   (APIENTRY *PFN_glDepthMask)(GLboolean flag);
typedef void   (APIENTRY *PFN_glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef GLenum (APIENTRY *PFN_glGetError)(void);
typedef const GLubyte* (APIENTRY *PFN_glGetString)(GLenum name);

// Buffers
typedef void   (APIENTRY *PFN_glGenBuffers)(GLsizei n, GLuint* buffers);
typedef void   (APIENTRY *PFN_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
typedef void   (APIENTRY *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void   (APIENTRY *PFN_glBufferData)(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
typedef void   (APIENTRY *PFN_glBufferSubData)(GLenum target, GLsizeiptr offset, GLsizeiptr size, const GLvoid* data);

// Vertex attributes
typedef void   (APIENTRY *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void   (APIENTRY *PFN_glDisableVertexAttribArray)(GLuint index);
typedef void   (APIENTRY *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef GLint  (APIENTRY *PFN_glGetAttribLocation)(GLuint program, const GLchar* name);

// Draw
typedef void   (APIENTRY *PFN_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
typedef void   (APIENTRY *PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);

// Shaders
typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum type);
typedef void   (APIENTRY *PFN_glDeleteShader)(GLuint shader);
typedef void   (APIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void   (APIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void   (APIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
typedef void   (APIENTRY *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);

// Program
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void   (APIENTRY *PFN_glDeleteProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void   (APIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glUseProgram)(GLuint program);
typedef void   (APIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
typedef void   (APIENTRY *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLint  (APIENTRY *PFN_glGetUniformLocation)(GLuint program, const GLchar* name);

// Uniforms
typedef void   (APIENTRY *PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void   (APIENTRY *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void   (APIENTRY *PFN_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void   (APIENTRY *PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (APIENTRY *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void   (APIENTRY *PFN_glUniform1i)(GLint location, GLint v0);

// Textures
typedef void   (APIENTRY *PFN_glGenTextures)(GLsizei n, GLuint* textures);
typedef void   (APIENTRY *PFN_glDeleteTextures)(GLsizei n, const GLuint* textures);
typedef void   (APIENTRY *PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void   (APIENTRY *PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
typedef void   (APIENTRY *PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void   (APIENTRY *PFN_glActiveTexture)(GLenum texture);
typedef void   (APIENTRY *PFN_glPixelStorei)(GLenum pname, GLint param);
typedef void   (APIENTRY *PFN_glBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void   (APIENTRY *PFN_glLineWidth)(GLfloat width);

// VAO (GL 3.0)
typedef void   (APIENTRY *PFN_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef void   (APIENTRY *PFN_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef void   (APIENTRY *PFN_glBindVertexArray)(GLuint array);
}

// ── Global function pointers ───────────────────────────────────────────────
inline PFN_glClear                   pglClear                   = nullptr;
inline PFN_glClearColor              pglClearColor              = nullptr;
inline PFN_glEnable                  pglEnable                  = nullptr;
inline PFN_glDisable                 pglDisable                 = nullptr;
inline PFN_glDepthFunc               pglDepthFunc               = nullptr;
inline PFN_glDepthMask               pglDepthMask               = nullptr;
inline PFN_glViewport                pglViewport                = nullptr;
inline PFN_glGetError                pglGetError                = nullptr;
inline PFN_glGetString               pglGetString               = nullptr;
inline PFN_glGenBuffers              pglGenBuffers              = nullptr;
inline PFN_glDeleteBuffers           pglDeleteBuffers           = nullptr;
inline PFN_glBindBuffer              pglBindBuffer              = nullptr;
inline PFN_glBufferData              pglBufferData              = nullptr;
inline PFN_glBufferSubData           pglBufferSubData           = nullptr;
inline PFN_glEnableVertexAttribArray pglEnableVertexAttribArray = nullptr;
inline PFN_glDisableVertexAttribArray pglDisableVertexAttribArray = nullptr;
inline PFN_glVertexAttribPointer     pglVertexAttribPointer     = nullptr;
inline PFN_glGetAttribLocation       pglGetAttribLocation       = nullptr;
inline PFN_glDrawElements            pglDrawElements            = nullptr;
inline PFN_glDrawArrays              pglDrawArrays              = nullptr;
inline PFN_glCreateShader            pglCreateShader            = nullptr;
inline PFN_glDeleteShader            pglDeleteShader            = nullptr;
inline PFN_glShaderSource            pglShaderSource            = nullptr;
inline PFN_glCompileShader           pglCompileShader           = nullptr;
inline PFN_glGetShaderiv             pglGetShaderiv             = nullptr;
inline PFN_glGetShaderInfoLog        pglGetShaderInfoLog        = nullptr;
inline PFN_glCreateProgram           pglCreateProgram           = nullptr;
inline PFN_glDeleteProgram           pglDeleteProgram           = nullptr;
inline PFN_glAttachShader            pglAttachShader            = nullptr;
inline PFN_glLinkProgram             pglLinkProgram             = nullptr;
inline PFN_glUseProgram              pglUseProgram              = nullptr;
inline PFN_glGetProgramiv            pglGetProgramiv            = nullptr;
inline PFN_glGetProgramInfoLog       pglGetProgramInfoLog       = nullptr;
inline PFN_glGetUniformLocation      pglGetUniformLocation      = nullptr;
inline PFN_glUniformMatrix4fv        pglUniformMatrix4fv        = nullptr;
inline PFN_glUniform4f               pglUniform4f               = nullptr;
inline PFN_glUniform3f               pglUniform3f               = nullptr;
inline PFN_glUniform2f               pglUniform2f               = nullptr;
inline PFN_glUniform1f               pglUniform1f               = nullptr;
inline PFN_glUniform1i               pglUniform1i               = nullptr;
inline PFN_glGenTextures             pglGenTextures             = nullptr;
inline PFN_glDeleteTextures          pglDeleteTextures          = nullptr;
inline PFN_glBindTexture             pglBindTexture             = nullptr;
inline PFN_glTexImage2D              pglTexImage2D              = nullptr;
inline PFN_glTexParameteri           pglTexParameteri           = nullptr;
inline PFN_glActiveTexture           pglActiveTexture           = nullptr;
inline PFN_glPixelStorei             pglPixelStorei             = nullptr;
inline PFN_glBlendFunc               pglBlendFunc               = nullptr;
inline PFN_glLineWidth               pglLineWidth               = nullptr;
inline PFN_glGenVertexArrays         pglGenVertexArrays         = nullptr;
inline PFN_glDeleteVertexArrays      pglDeleteVertexArrays      = nullptr;
inline PFN_glBindVertexArray         pglBindVertexArray         = nullptr;

// ── Load all GL functions ──────────────────────────────────────────────────
inline bool gl_load_extensions() {
    // GL 1.1 functions (from opengl32.dll directly)
    pglClear      = (PFN_glClear)SDL_GL_GetProcAddress("glClear");
    pglClearColor = (PFN_glClearColor)SDL_GL_GetProcAddress("glClearColor");
    pglEnable     = (PFN_glEnable)SDL_GL_GetProcAddress("glEnable");
    pglDisable    = (PFN_glDisable)SDL_GL_GetProcAddress("glDisable");
    pglDepthFunc  = (PFN_glDepthFunc)SDL_GL_GetProcAddress("glDepthFunc");
    pglDepthMask  = (PFN_glDepthMask)SDL_GL_GetProcAddress("glDepthMask");
    pglViewport   = (PFN_glViewport)SDL_GL_GetProcAddress("glViewport");
    pglGetError   = (PFN_glGetError)SDL_GL_GetProcAddress("glGetError");
    pglGetString  = (PFN_glGetString)SDL_GL_GetProcAddress("glGetString");
    pglDrawElements = (PFN_glDrawElements)SDL_GL_GetProcAddress("glDrawElements");
    pglDrawArrays   = (PFN_glDrawArrays)SDL_GL_GetProcAddress("glDrawArrays");

    // GL 1.5+ / 2.0+ functions
    pglGenBuffers              = (PFN_glGenBuffers)SDL_GL_GetProcAddress("glGenBuffers");
    pglDeleteBuffers           = (PFN_glDeleteBuffers)SDL_GL_GetProcAddress("glDeleteBuffers");
    pglBindBuffer              = (PFN_glBindBuffer)SDL_GL_GetProcAddress("glBindBuffer");
    pglBufferData              = (PFN_glBufferData)SDL_GL_GetProcAddress("glBufferData");
    pglBufferSubData           = (PFN_glBufferSubData)SDL_GL_GetProcAddress("glBufferSubData");
    pglEnableVertexAttribArray = (PFN_glEnableVertexAttribArray)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    pglDisableVertexAttribArray = (PFN_glDisableVertexAttribArray)SDL_GL_GetProcAddress("glDisableVertexAttribArray");
    pglVertexAttribPointer     = (PFN_glVertexAttribPointer)SDL_GL_GetProcAddress("glVertexAttribPointer");
    pglGetAttribLocation       = (PFN_glGetAttribLocation)SDL_GL_GetProcAddress("glGetAttribLocation");

    // GL 2.0 functions
    pglCreateShader            = (PFN_glCreateShader)SDL_GL_GetProcAddress("glCreateShader");
    pglDeleteShader            = (PFN_glDeleteShader)SDL_GL_GetProcAddress("glDeleteShader");
    pglShaderSource            = (PFN_glShaderSource)SDL_GL_GetProcAddress("glShaderSource");
    pglCompileShader           = (PFN_glCompileShader)SDL_GL_GetProcAddress("glCompileShader");
    pglGetShaderiv             = (PFN_glGetShaderiv)SDL_GL_GetProcAddress("glGetShaderiv");
    pglGetShaderInfoLog        = (PFN_glGetShaderInfoLog)SDL_GL_GetProcAddress("glGetShaderInfoLog");
    pglCreateProgram           = (PFN_glCreateProgram)SDL_GL_GetProcAddress("glCreateProgram");
    pglDeleteProgram           = (PFN_glDeleteProgram)SDL_GL_GetProcAddress("glDeleteProgram");
    pglAttachShader            = (PFN_glAttachShader)SDL_GL_GetProcAddress("glAttachShader");
    pglLinkProgram             = (PFN_glLinkProgram)SDL_GL_GetProcAddress("glLinkProgram");
    pglUseProgram              = (PFN_glUseProgram)SDL_GL_GetProcAddress("glUseProgram");
    pglGetProgramiv            = (PFN_glGetProgramiv)SDL_GL_GetProcAddress("glGetProgramiv");
    pglGetProgramInfoLog       = (PFN_glGetProgramInfoLog)SDL_GL_GetProcAddress("glGetProgramInfoLog");
    pglGetUniformLocation      = (PFN_glGetUniformLocation)SDL_GL_GetProcAddress("glGetUniformLocation");
    pglUniformMatrix4fv        = (PFN_glUniformMatrix4fv)SDL_GL_GetProcAddress("glUniformMatrix4fv");
    pglUniform4f               = (PFN_glUniform4f)SDL_GL_GetProcAddress("glUniform4f");
    pglUniform3f               = (PFN_glUniform3f)SDL_GL_GetProcAddress("glUniform3f");
    pglUniform2f               = (PFN_glUniform2f)SDL_GL_GetProcAddress("glUniform2f");
    pglUniform1f               = (PFN_glUniform1f)SDL_GL_GetProcAddress("glUniform1f");
    pglUniform1i               = (PFN_glUniform1i)SDL_GL_GetProcAddress("glUniform1i");

    // Textures (GL 1.1 / 1.3)
    pglGenTextures    = (PFN_glGenTextures)SDL_GL_GetProcAddress("glGenTextures");
    pglDeleteTextures = (PFN_glDeleteTextures)SDL_GL_GetProcAddress("glDeleteTextures");
    pglBindTexture    = (PFN_glBindTexture)SDL_GL_GetProcAddress("glBindTexture");
    pglTexImage2D     = (PFN_glTexImage2D)SDL_GL_GetProcAddress("glTexImage2D");
    pglTexParameteri  = (PFN_glTexParameteri)SDL_GL_GetProcAddress("glTexParameteri");
    pglActiveTexture  = (PFN_glActiveTexture)SDL_GL_GetProcAddress("glActiveTexture");
    pglPixelStorei    = (PFN_glPixelStorei)SDL_GL_GetProcAddress("glPixelStorei");
    pglBlendFunc      = (PFN_glBlendFunc)SDL_GL_GetProcAddress("glBlendFunc");
    pglLineWidth      = (PFN_glLineWidth)SDL_GL_GetProcAddress("glLineWidth");

    // GL 3.0 functions
    pglGenVertexArrays    = (PFN_glGenVertexArrays)SDL_GL_GetProcAddress("glGenVertexArrays");
    pglDeleteVertexArrays = (PFN_glDeleteVertexArrays)SDL_GL_GetProcAddress("glDeleteVertexArrays");
    pglBindVertexArray    = (PFN_glBindVertexArray)SDL_GL_GetProcAddress("glBindVertexArray");

    if (!pglCreateProgram || !pglDrawElements || !pglGenBuffers || !pglBindVertexArray) {
        SDL_Log("CRITICAL: Some GL functions failed to load!");
        return false;
    }

    return true;
}

#endif // GL_EXT_H
