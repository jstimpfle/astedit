#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/gfx.h>
#include <astedit/window.h>
#include <stddef.h>  // offsetof()



/* To include <GL/gl.h>, <GL/glu.h>, and <GL/glext.h> on windows, it seems we
 * are required to include <windows.h> first, or else we'll get strange error
 * messages */
 /* Note that <GL/glext.h> is not provided by Microsoft. I've downloaded it from
  * Khronos and stored it in-tree */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

  /* XXX: Load GLFW (our OpenGL context creator library) as well. We use
   * glfwGetProcAddress. Maybe later remove this dependency and write a few lines
   * of platform code instead? (glXGetProcAddress(), wglGetProcAddress()). */
#include <GLFW/glfw3.h>


#define CHECK_GL_ERRORS() check_gl_errors(__FILE__, __LINE__)


struct OpenGLInitInfo {
        void(**funcptr)(void);
        const char *name;
};

/* Define function pointers for all OpenGL extensions that we want to load */
#define MAKE(tp, name) static tp name;
#include <astedit/opengl-extensions.inc>
#undef MAKE

/* Define initialization info for all OpenGL extensions that we want to load */
const struct OpenGLInitInfo openGLInitInfo[] = {
#define MAKE(tp, name)  { (void(**)(void)) &name, #name },
#include <astedit/opengl-extensions.inc>
#undef MAKE
};

static void load_opengl_function_pointers(void)
{
        for (int i = 0; i < LENGTH(openGLInitInfo); i++) {
                const char *name = openGLInitInfo[i].name;
                void(**funcptr)(void) = openGLInitInfo[i].funcptr;

                *funcptr = glfwGetProcAddress(name);
                if (!*funcptr)
                        fatalf("OpenGL extension %s not found\n", name);
        }
}


struct Mat4 {
        GLfloat m[4][4];
};

enum {
        SHADER_VERTEXVARYINGCOLOR,
        SHADER_FRAGMENTVARYINGCOLOR,
        SHADER_VERTEXTEXTURE,
        SHADER_FRAGMENTTEXTUREALPHA,
        SHADER_FRAGMENTTEXTURERGBA,
        SHADER_FRAGMENTTEXTURERGBDUALSOURCE,
        NUM_SHADER_KINDS,
};

enum {
        PROGRAM_VARYINGCOLOR,
        PROGRAM_TEXTUREALPHA,
        PROGRAM_TEXTURERGBA,
        PROGRAM_TEXTURERGBDUALSOURCE,
        NUM_PROGRAM_KINDS,
};

enum {
        UNIFORM_VARYINGCOLOR_mat,
        UNIFORM_TEXTUREALPHA_mat,
        UNIFORM_TEXTUREALPHA_sampler,
        UNIFORM_TEXTURERGBA_mat,
        UNIFORM_TEXTURERGBA_sampler,
        UNIFORM_TEXTURERGBDUALSOURCE_mat,
        UNIFORM_TEXTURERGBDUALSOURCE_sampler,
        NUM_UNIFORM_KINDS,
};

enum {
        ATTRIB_VARYINGCOLOR_pos,
        ATTRIB_VARYINGCOLOR_color,
        ATTRIB_TEXTUREALPHA_pos,
        ATTRIB_TEXTUREALPHA_color,
        ATTRIB_TEXTUREALPHA_texPos,
        ATTRIB_TEXTURERGBA_pos,
        ATTRIB_TEXTURERGBA_texPos,
        ATTRIB_TEXTURERGBDUALSOURCE_pos,
        ATTRIB_TEXTURERGBDUALSOURCE_color,
        NUM_ATTRIB_KINDS,
};

struct ShaderInfo {
        const char *shaderName;
        const char *shaderSource;
        int glshaderKind;
};

/* associate a program with a shader */
struct ShaderLinkInfo {
        int programKind;
        int shaderKind;
};

/* associate a program with a uniform (a "constant") */
struct UniformInfo {
        int programKind;  // PROGRAM_??
        const char *uniformName;
};

/* associate a program with an attribute (a variable) */
struct AttribInfo {
        int programKind;  // PROGRAM_??
        const char *attribName;
};

const struct ShaderInfo shaderInfo[NUM_SHADER_KINDS] = {
#define MAKE(x, y, z) [x] = { #x, z, y }
        MAKE(SHADER_VERTEXVARYINGCOLOR, GL_VERTEX_SHADER,
              "#version 130\n"
              "in vec3 pos;\n"
              "in vec4 color;\n"
              "uniform mat4 mat;\n"
              "out vec4 colorF;\n"
              "out vec2 texPosF;\n"
              "void main()\n"
              "{\n"
              "    colorF = color;\n"
              "    gl_Position = mat * vec4(pos, 1.0);\n"
              "}\n"),
        MAKE(SHADER_FRAGMENTVARYINGCOLOR, GL_FRAGMENT_SHADER,
              "#version 130\n"
              "in vec4 colorF;\n"
              "void main()\n"
              "{\n"
              "    gl_FragColor = colorF;\n"
              "}\n"),
        MAKE(SHADER_VERTEXTEXTURE, GL_VERTEX_SHADER,
              "#version 130\n"
              "in vec3 pos;\n"
              "in vec4 color;\n"
              "in vec2 texPos;\n"
              "uniform mat4 mat;\n"
              "out vec4 colorF;\n"
              "out vec2 texPosF;\n"
              "void main()\n"
              "{\n"
              "    colorF = color;\n"
              "    texPosF = texPos;\n"
              "    gl_Position = mat * vec4(pos, 1.0);\n"
              "}\n"),
        MAKE(SHADER_FRAGMENTTEXTUREALPHA, GL_FRAGMENT_SHADER,
              "#version 130\n"
              "in vec4 colorF;\n"
              "in vec2 texPosF;\n"
              "uniform sampler2D sampler;\n"
              "void main()\n"
              "{\n"
              "    vec3 t = texture(sampler, texPosF).rgb;\n"
              //"    gl_FragColor = vec4(colorF.r * t.r, colorF.g * t.g, colorF.b * t.b, 1.0);\n"
              "    gl_FragColor = vec4(colorF.rgb * t, (t.r+t.g+t.b)*colorF.a);\n"
              "}\n"),
        MAKE(SHADER_FRAGMENTTEXTURERGBA, GL_FRAGMENT_SHADER,
              "#version 130\n"
              "in vec2 texPosF;\n"
              "uniform sampler2D sampler;\n"
              "void main()\n"
              "{\n"
              "    gl_FragColor = texture(sampler, texPosF);\n"
              "}\n"),
        MAKE(SHADER_FRAGMENTTEXTURERGBDUALSOURCE, GL_FRAGMENT_SHADER,
             "#version 330\n"
             "out vec4 outColor;\n"
             "out vec4 outBlend;  /* blend factors for individual colors in outColor (Dual Source Blending) */\n"
             "in vec4 colorF;\n"
             "in vec2 texPosF;\n"
             "uniform sampler2D sampler;\n"
             "void main()\n"
             "{\n"
             "    vec4 t = texture(sampler, texPosF);\n"
             "    outColor = colorF;\n"
             "    outBlend = vec4(t.rgb, 1.0);\n"
             "}\n"),
#undef MAKE
};

const char *const programKindString[NUM_PROGRAM_KINDS] = {
#define MAKE(x) [x] = #x
        MAKE(PROGRAM_VARYINGCOLOR),
        MAKE(PROGRAM_TEXTUREALPHA),
        MAKE(PROGRAM_TEXTURERGBA),
#undef MAKE
};

const struct ShaderLinkInfo shaderLinkInfo[] = {
        { PROGRAM_VARYINGCOLOR, SHADER_VERTEXVARYINGCOLOR },
        { PROGRAM_VARYINGCOLOR, SHADER_FRAGMENTVARYINGCOLOR },
        { PROGRAM_TEXTUREALPHA, SHADER_VERTEXTEXTURE },
        { PROGRAM_TEXTUREALPHA, SHADER_FRAGMENTTEXTUREALPHA },
        { PROGRAM_TEXTURERGBA, SHADER_VERTEXTEXTURE },
        { PROGRAM_TEXTURERGBA, SHADER_FRAGMENTTEXTURERGBA },
        { PROGRAM_TEXTURERGBDUALSOURCE, SHADER_VERTEXTEXTURE },
        { PROGRAM_TEXTURERGBDUALSOURCE, SHADER_FRAGMENTTEXTURERGBDUALSOURCE },
};

const struct UniformInfo uniformInfo[NUM_UNIFORM_KINDS] = {
        [UNIFORM_VARYINGCOLOR_mat] = { PROGRAM_VARYINGCOLOR, "mat" },
        [UNIFORM_TEXTUREALPHA_mat] = { PROGRAM_TEXTUREALPHA, "mat" },
        [UNIFORM_TEXTUREALPHA_sampler] = { PROGRAM_TEXTUREALPHA, "sampler" },
        [UNIFORM_TEXTURERGBA_mat] = { PROGRAM_TEXTURERGBA, "mat" },
        [UNIFORM_TEXTURERGBA_sampler] = { PROGRAM_TEXTURERGBA, "sampler" },
        [UNIFORM_TEXTURERGBDUALSOURCE_mat] = { PROGRAM_TEXTURERGBDUALSOURCE, "mat" },
        [UNIFORM_TEXTURERGBDUALSOURCE_sampler] = { PROGRAM_TEXTURERGBDUALSOURCE, "sampler" },
};

const struct AttribInfo attribInfo[NUM_ATTRIB_KINDS] = {
        [ATTRIB_VARYINGCOLOR_pos] = { PROGRAM_VARYINGCOLOR, "pos" },
        [ATTRIB_VARYINGCOLOR_color] = { PROGRAM_VARYINGCOLOR, "color" },
        [ATTRIB_TEXTUREALPHA_pos] = { PROGRAM_TEXTUREALPHA, "pos" },
        [ATTRIB_TEXTUREALPHA_color] = { PROGRAM_TEXTUREALPHA, "color" },
        [ATTRIB_TEXTUREALPHA_texPos] = { PROGRAM_TEXTUREALPHA, "texPos" },
        [ATTRIB_TEXTURERGBA_pos] = { PROGRAM_TEXTURERGBA, "pos" },
        [ATTRIB_TEXTURERGBA_texPos] = { PROGRAM_TEXTURERGBA, "texPos" },
        [ATTRIB_TEXTURERGBDUALSOURCE_pos] = { PROGRAM_TEXTURERGBDUALSOURCE, "pos" },
        [ATTRIB_TEXTURERGBDUALSOURCE_color] = { PROGRAM_TEXTURERGBDUALSOURCE, "color" },
};


static GLuint vaoOfProgram[NUM_PROGRAM_KINDS];
static GLuint vbo;

static GLint shader_GL_id[NUM_SHADER_KINDS];
static GLint program_GL_id[NUM_PROGRAM_KINDS];
static GLint uniformLocation[NUM_UNIFORM_KINDS];
static GLint attribLocation[NUM_ATTRIB_KINDS];

/* Matrix for coordinate system: top-left = (0,0). bottom-right = some other
point */
static struct Mat4 transformMatrix;


static int srgbEnabled = 1;


static void check_gl_errors(const char *filename, int line)
{
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
                fatalf("In %s line %d: GL error %s\n", filename, line,
                        gluErrorString(err));
}

static int get_compile_status(GLint shader, const char *name)
{
        GLint compileStatus;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
        if (compileStatus != GL_TRUE) {
                GLchar errorBuf[1024];
                GLsizei length;
                glGetShaderInfoLog(shader, sizeof errorBuf, &length, errorBuf);
                fatalf("Warning: shader %s failed to compile: %s\n",
                        name, errorBuf);
        }
        return compileStatus == GL_TRUE;
}

static int get_link_status(GLint program)
{
        GLint linkStatus;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
                GLsizei length;
                GLchar errorBuf[128];
                glGetProgramInfoLog(program, sizeof errorBuf,
                        &length, errorBuf);
                log_postf("Warning: Failed to link shader program %d: %s\n",
                        program, errorBuf);
        }
        return linkStatus == GL_TRUE;
}

void setup_gfx(void)
{
        {
                GLint ctx_glMajorVersion;
                GLint ctx_glMinorVersion;
                /* This interface only exists in OpenGL 3.0 and higher */
                glGetIntegerv(GL_MAJOR_VERSION, &ctx_glMajorVersion);
                glGetIntegerv(GL_MINOR_VERSION, &ctx_glMinorVersion);
                log_postf("OpenGL version: %d.%d\n",
                        (int)ctx_glMajorVersion,
                        (int)ctx_glMinorVersion);
        }

        load_opengl_function_pointers();

        /*
         * Create, load, and compile shaders
         */

        for (int i = 0; i < NUM_SHADER_KINDS; i++)
                shader_GL_id[i] = glCreateShader(shaderInfo[i].glshaderKind);
        CHECK_GL_ERRORS();

        for (int i = 0; i < NUM_SHADER_KINDS; i++)
                glShaderSource(shader_GL_id[i], 1,
                        &shaderInfo[i].shaderSource, NULL);
        CHECK_GL_ERRORS();

        for (int i = 0; i < NUM_SHADER_KINDS; i++)
                glCompileShader(shader_GL_id[i]);
        CHECK_GL_ERRORS();

        for (int i = 0; i < NUM_SHADER_KINDS; i++) {
                if (!get_compile_status(shader_GL_id[i], shaderInfo[i].shaderName)) {
                        CHECK_GL_ERRORS();
                        fatalf("Failed to compile vertex shader: %s!\n",
                                shaderInfo[i].shaderName);
                }
        }
        CHECK_GL_ERRORS();



        /*
         * Create programs, attach shaders, link programs
         */

        for (int i = 0; i < NUM_PROGRAM_KINDS; i++) {
                program_GL_id[i] = glCreateProgram();
        }
        CHECK_GL_ERRORS();

        for (int i = 0; i < LENGTH(shaderLinkInfo); i++) {
                int program = shaderLinkInfo[i].programKind;
                int shader = shaderLinkInfo[i].shaderKind;
                glAttachShader(program_GL_id[program], shader_GL_id[shader]);
        }
        CHECK_GL_ERRORS();

        /* TODO: move this to descriptive structs as well */
        glBindFragDataLocationIndexed(program_GL_id[PROGRAM_TEXTURERGBDUALSOURCE], 0, 0, "outColor");
        glBindFragDataLocationIndexed(program_GL_id[PROGRAM_TEXTURERGBDUALSOURCE], 0, 1, "outBlend");

        for (int i = 0; i < NUM_PROGRAM_KINDS; i++) {
                glLinkProgram(program_GL_id[i]);
                CHECK_GL_ERRORS();
                if (!get_link_status(program_GL_id[i])) {
                        fatalf("Failed to link shader program %s!\n",
                                programKindString[i]);
                }
        }
        CHECK_GL_ERRORS();

        /*
         * Query locations of uniforms and attributes in programs
         */

        for (int i = 0; i < NUM_UNIFORM_KINDS; i++) {
                int programKind = uniformInfo[i].programKind;
                const char *uniformName = uniformInfo[i].uniformName;
                GLint loc = glGetUniformLocation(program_GL_id[programKind],
                        uniformName);
                if (loc == -1) {
                        log_postf("Warning: Failed to query uniform "
                                "\"%s\" of shader program %s\n",
                                uniformName, programKindString[programKind]);
                }
                uniformLocation[i] = loc;
        }
        CHECK_GL_ERRORS();

        for (int i = 0; i < NUM_ATTRIB_KINDS; i++) {
                int programKind = attribInfo[i].programKind;
                const char *attribName = attribInfo[i].attribName;
                GLint loc = glGetAttribLocation(program_GL_id[programKind],
                        attribName);
                if (loc == -1) {
                        log_postf("Warning: Failed to query attrib "
                                "\"%s\" of shader program %s\n",
                                attribName, programKindString[programKind]);
                }
                attribLocation[i] = loc;
        }
        CHECK_GL_ERRORS();

        /*
         * Setup VAOs
         */

        glGenVertexArrays(NUM_PROGRAM_KINDS, &vaoOfProgram[0]);
        glGenBuffers(1, &vbo);  // one buffer for all, currently. Buffer gets overwritten on each draw
        CHECK_GL_ERRORS();

        glBindVertexArray(vaoOfProgram[PROGRAM_VARYINGCOLOR]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_VARYINGCOLOR_pos]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_VARYINGCOLOR_color]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(attribLocation[ATTRIB_VARYINGCOLOR_pos],
                3, GL_FLOAT, GL_FALSE, sizeof(struct ColorVertex2d),
                (char *)0 + offsetof(struct ColorVertex2d, x));
        glVertexAttribPointer(attribLocation[ATTRIB_VARYINGCOLOR_color],
                4, GL_FLOAT, GL_FALSE, sizeof(struct ColorVertex2d),
                (char *)0 + offsetof(struct ColorVertex2d, r));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glBindVertexArray(vaoOfProgram[PROGRAM_TEXTUREALPHA]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_TEXTUREALPHA_pos]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_TEXTUREALPHA_color]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_TEXTUREALPHA_texPos]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(attribLocation[ATTRIB_TEXTUREALPHA_pos],
                3, GL_FLOAT, GL_FALSE, sizeof(struct TextureVertex2d),
                (char *)0 + offsetof(struct TextureVertex2d, x));
        glVertexAttribPointer(attribLocation[ATTRIB_TEXTUREALPHA_color],
                4, GL_FLOAT, GL_FALSE, sizeof(struct TextureVertex2d),
                (char *)0 + offsetof(struct TextureVertex2d, r));
        glVertexAttribPointer(attribLocation[ATTRIB_TEXTUREALPHA_texPos],
                2, GL_FLOAT, GL_FALSE, sizeof(struct TextureVertex2d),
                (char *)0 + offsetof(struct TextureVertex2d, texX));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glBindVertexArray(vaoOfProgram[PROGRAM_TEXTURERGBA]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_TEXTURERGBA_pos]);
        glEnableVertexAttribArray(attribLocation[ATTRIB_TEXTURERGBA_texPos]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(attribLocation[ATTRIB_TEXTURERGBA_pos],
                3, GL_FLOAT, GL_FALSE, sizeof(struct TextureVertex2d),
                (char *)0 + offsetof(struct TextureVertex2d, x));
        glVertexAttribPointer(attribLocation[ATTRIB_TEXTURERGBA_texPos],
                2, GL_FLOAT, GL_FALSE, sizeof(struct TextureVertex2d),
                (char *)0 + offsetof(struct TextureVertex2d, texX));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        CHECK_GL_ERRORS();
}

void teardown_gfx(void)
{
        for (int i = 0; i < NUM_PROGRAM_KINDS; i++)
                glDeleteProgram(program_GL_id[i]);
        for (int i = 0; i < NUM_SHADER_KINDS; i++)
                glDeleteShader(shader_GL_id[i]);
        /* TODO: which textures are still alive? */
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(NUM_PROGRAM_KINDS, &vaoOfProgram[0]);
        glFlush();
        CHECK_GL_ERRORS();
}


void upload_rgba_texture_data(Texture texture, const unsigned char *data, int size, int w, int h)
{
        ENSURE(size == w * h * 4);
        UNUSED(size);

        glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
        /*
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        */

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        CHECK_GL_ERRORS();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERRORS();

}

void upload_rgb_texture_data(Texture texture, const unsigned char *data, int size, int w, int h)
{
        //ENSURE(data != NULL);  // From now on, NULL is valid and means to allocate the texture without any initialization pixel data
        ENSURE(texture >= 0);
        ENSURE(size == 3 * w * h);
        UNUSED(size);

        glBindTexture(GL_TEXTURE_2D, (GLuint)texture);

        /*
         * Setup textures
         */

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERRORS();
}

void upload_alpha_texture_data(Texture texture, const unsigned char *data, int size, int w, int h)
{
        //ENSURE(data != NULL);  // From now on, NULL is valid and means to allocate the texture without any initialization pixel data
        ENSURE(texture >= 0);
        ENSURE(size == w * h);
        UNUSED(size);

        glBindTexture(GL_TEXTURE_2D, (GLuint)texture);

        /*
         * Setup textures
         */

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERRORS();
}

void update_alpha_texture_subimage(Texture texture, int row, int numRows, int rowWidth, int stride, const unsigned char *data)
{
        ENSURE(!(stride & 3));
        UNUSED(stride);
        glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, rowWidth, numRows, GL_RED, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERRORS();
}

void update_rgb_texture_subimage(Texture texture, int y, int h, int w, int stride, const unsigned char *data)
{
        //log_postf("glTexSubImage2D(y=%d, h=%d, w=%d, stride=%d)\n", y, h, w, stride);
        ENSURE(!(stride & 3));
        ENSURE(w % 3 == 0);
        UNUSED(stride);
        glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w/3, h, GL_RGB, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERRORS();
}


Texture create_rgba_texture(int w, int h)
{
        GLuint tex;
        glGenTextures(1, &tex);
        upload_rgba_texture_data(tex, NULL, 4 * w * h, w, h);
        return tex;
}


Texture create_alpha_texture(int w, int h)
{
        GLuint tex;
        glGenTextures(1, &tex);
        upload_alpha_texture_data(tex, NULL, w * h, w, h);
        return tex;
}

Texture create_rgb_texture(int w, int h)
{
        GLuint tex;
        glGenTextures(1, &tex);
        upload_rgb_texture_data(tex, NULL, 3 * w * h, w, h);
        return tex;
}

void destroy_texture(Texture texHandle)
{
        ENSURE((Texture)(GLuint)texHandle == texHandle);
        GLuint tex = (GLuint)texHandle;
        glDeleteTextures(1, &tex);
        CHECK_GL_ERRORS();
}

void set_2d_coordinate_system(int x, int y, int w, int h)
{
        (void)x;
        (void)y;
        struct Mat4 mat = {0};
        mat.m[0][0] = 2.0f / w;
        mat.m[1][1] = -2.0f / h;
        mat.m[2][2] = 1.0f;
        mat.m[3][3] = 1.0f;
        mat.m[0][3] = -1.0f;
        mat.m[1][3] = 1.0f;
        transformMatrix = mat;
}

void set_clipping_rect_in_pixels(int x, int y, int w, int h)
{
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, windowHeightInPixels - y - h, w, h);
        CHECK_GL_ERRORS();
}

void clear_clipping_rect(void)
{
        glDisable(GL_SCISSOR_TEST);
}

void set_viewport_in_pixels(int x, int y, int w, int h)
{
        int Cx = x;
        int Cy = windowHeightInPixels - y - h;
        int Cw = w;
        int Ch = h;
        glViewport(Cx, Cy, Cw, Ch);
        CHECK_GL_ERRORS();
}

void gfx_toggle_srgb(void)
{
        srgbEnabled ^= 1;
        log_postf("srgbEnabled: %d\n", srgbEnabled);
}

void clear_screen_and_drawing_state(void)
{
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(-1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (srgbEnabled)
                glEnable(GL_FRAMEBUFFER_SRGB);
        else
                glDisable(GL_FRAMEBUFFER_SRGB);

        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_DEPTH_TEST);
        //glEnable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glDepthFunc(GL_GEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        CHECK_GL_ERRORS();
}

void flush_gfx(void)
{
        glFlush();
        CHECK_GL_ERRORS();
}

void draw_rgba_vertices(struct ColorVertex2d *verts, int numVerts)
{
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        CHECK_GL_ERRORS();
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof *verts, verts,
                GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_GL_ERRORS();

        glUseProgram(program_GL_id[PROGRAM_VARYINGCOLOR]);
        glUniformMatrix4fv(uniformLocation[UNIFORM_VARYINGCOLOR_mat],
                1, GL_TRUE, &transformMatrix.m[0][0]);
        glBindVertexArray(vaoOfProgram[PROGRAM_VARYINGCOLOR]);
        glDrawArrays(GL_TRIANGLES, 0, numVerts);
        glBindVertexArray(0);
        glUseProgram(0);
        CHECK_GL_ERRORS();
}

void draw_rgba_texture_vertices(struct TextureVertex2d *verts, int numVerts)
{
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof *verts, verts,
                GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glUseProgram(program_GL_id[PROGRAM_TEXTURERGBA]);
        glBindVertexArray(vaoOfProgram[PROGRAM_TEXTURERGBA]);
        glUniformMatrix4fv(uniformLocation[UNIFORM_TEXTURERGBA_mat], 1, GL_TRUE, &transformMatrix.m[0][0]);
        glUniform1i(uniformLocation[UNIFORM_TEXTURERGBA_sampler], 0/*texture unit 0 ???*/);

        int pos = 0;
        while (pos < numVerts) {
                GLuint texture = (GLuint)verts[pos].tex;
                int num = 0;
                while (pos + num < numVerts && verts[pos + num].tex == verts[pos].tex)
                        num++;
                glBindTexture(GL_TEXTURE_2D, texture);
                glDrawArrays(GL_TRIANGLES, pos, num);
                glBindTexture(GL_TEXTURE_2D, 0);
                CHECK_GL_ERRORS();
                pos += num;
        }
        glBindVertexArray(0);
        glUseProgram(0);
        CHECK_GL_ERRORS();
}

void draw_alpha_texture_vertices(struct TextureVertex2d *verts, int numVerts)
{
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof *verts, verts, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        //glUseProgram(program_GL_id[PROGRAM_TEXTUREALPHA]);
        glUseProgram(program_GL_id[PROGRAM_TEXTURERGBDUALSOURCE]);

        glUniformMatrix4fv(uniformLocation[UNIFORM_TEXTURERGBDUALSOURCE_mat], 1, GL_TRUE, &transformMatrix.m[0][0]);
        glUniform1i(uniformLocation[UNIFORM_TEXTURERGBDUALSOURCE_sampler], 0/*texture unit 0 ???*/);

        glBindVertexArray(vaoOfProgram[PROGRAM_TEXTUREALPHA]); //XXX we still don't have a separate rgbdualsource VAO
        glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);

        int pos = 0;
        while (pos < numVerts) {
                GLuint texture = (GLuint)verts[pos].tex;
                int num = 0;
                while (pos + num < numVerts && verts[pos + num].tex == verts[pos].tex)
                        num++;
                glBindTexture(GL_TEXTURE_2D, texture);
                glDrawArrays(GL_TRIANGLES, pos, num);
                glBindTexture(GL_TEXTURE_2D, 0);
                CHECK_GL_ERRORS();
                pos += num;
        }
        glBindVertexArray(0);
        glUseProgram(0);
        CHECK_GL_ERRORS();
}
