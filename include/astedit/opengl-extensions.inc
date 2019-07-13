/*
 * OpenGL extensions that we need. We load them manually. No need for something
 * like GLEW. Note that Core OpenGL functions (as opposed to extensions) are not
 * listed here. They are defined in GL/gl.h and the OS's dynamic loader will
 * link them in at program startup. There is no need to declare them here.
 */

#ifdef _WIN32
/*
 * Only needed under windows. On my Linux boxes at least, OpenGL 1.3 is included
 * in the gl.h header. So we should not define a pointer symbol or load
 * anything. Just link against OpenGL.
 */
MAKE( PFNGLACTIVETEXTUREPROC,            glActiveTexture )
#endif

MAKE( PFNGLATTACHSHADERPROC,             glAttachShader )
MAKE( PFNGLBINDBUFFERPROC,               glBindBuffer )
MAKE( PFNGLBINDVERTEXARRAYPROC,          glBindVertexArray )
MAKE( PFNGLBUFFERDATAPROC,               glBufferData )
MAKE( PFNGLCOMPILESHADERPROC,            glCompileShader )
MAKE( PFNGLCREATEPROGRAMPROC,            glCreateProgram )
MAKE( PFNGLCREATESHADERPROC,             glCreateShader )
MAKE( PFNGLDELETEBUFFERSPROC,            glDeleteBuffers )
MAKE( PFNGLDELETEPROGRAMPROC,            glDeleteProgram )
MAKE( PFNGLDELETESHADERPROC,             glDeleteShader )
MAKE( PFNGLDELETEVERTEXARRAYSPROC,       glDeleteVertexArrays )
MAKE( PFNGLENABLEVERTEXATTRIBARRAYPROC,  glEnableVertexAttribArray )
MAKE( PFNGLGENBUFFERSPROC,               glGenBuffers )
MAKE( PFNGLGENVERTEXARRAYSPROC,          glGenVertexArrays )
MAKE( PFNGLGENERATEMIPMAPPROC,           glGenerateMipmap )
MAKE( PFNGLGETATTRIBLOCATIONPROC,        glGetAttribLocation )
MAKE( PFNGLGETPROGRAMINFOLOGPROC,        glGetProgramInfoLog )
MAKE( PFNGLGETPROGRAMIVPROC,             glGetProgramiv )
MAKE( PFNGLGETSHADERINFOLOGPROC,         glGetShaderInfoLog )
MAKE( PFNGLGETSHADERIVPROC,              glGetShaderiv )
MAKE( PFNGLGETUNIFORMLOCATIONPROC,       glGetUniformLocation )
MAKE( PFNGLLINKPROGRAMPROC,              glLinkProgram )
MAKE( PFNGLSHADERSOURCEPROC,             glShaderSource )
MAKE( PFNGLUNIFORMMATRIX4FVPROC,         glUniformMatrix4fv )
MAKE( PFNGLUNIFORM1IPROC,                glUniform1i )
MAKE( PFNGLUSEPROGRAMPROC,               glUseProgram )
MAKE( PFNGLVERTEXATTRIBPOINTERPROC,      glVertexAttribPointer )
