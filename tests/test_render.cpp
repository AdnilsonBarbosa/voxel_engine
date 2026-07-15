// Single-file test — validates every step and logs to test_output.txt.
// If this works, the problem is in the multi-file structure.
// If this fails, we know exactly where.

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stddef.h>

// ── Minimal GL loader (everything inline, no headers from Windows SDK) ─────
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  typedef void   (APIENTRY *PFNClear)(unsigned int mask);
  typedef void   (APIENTRY *PFNClearColor)(float r, float g, float b, float a);
  typedef void   (APIENTRY *PFNEnable)(unsigned int cap);
  typedef void   (APIENTRY *PFNDepthFunc)(unsigned int func);
  typedef void   (APIENTRY *PFNViewport)(int x, int y, int w, int h);
  typedef const unsigned char* (APIENTRY *PFNGetString)(unsigned int name);
  typedef unsigned int (APIENTRY *PFNGetError)(void);
  typedef void   (APIENTRY *PFNGenBuffers)(int n, unsigned int* bufs);
  typedef void   (APIENTRY *PFNBindBuffer)(unsigned int target, unsigned int buf);
  typedef void   (APIENTRY *PFNBufferData)(unsigned int target, long size, const void* data, unsigned int usage);
  typedef void   (APIENTRY *PFNGenVertexArrays)(int n, unsigned int* arrays);
  typedef void   (APIENTRY *PFNBindVertexArray)(unsigned int array);
  typedef void   (APIENTRY *PFNEnableVertexAttribArray)(unsigned int index);
  typedef void   (APIENTRY *PFNVertexAttribPointer)(unsigned int index, int size, unsigned int type, unsigned char norm, int stride, const void* ptr);
  typedef unsigned int (APIENTRY *PFNGetAttribLocation)(unsigned int prog, const char* name);
  typedef int    (APIENTRY *PFNGetUniformLocation)(unsigned int prog, const char* name);
  typedef void   (APIENTRY *PFNUniformMatrix4fv)(int loc, int count, unsigned char transpose, const float* value);
  typedef void   (APIENTRY *PFNUniform4f)(int loc, float a, float b, float c, float d);
  typedef void   (APIENTRY *PFNUseProgram)(unsigned int prog);
  typedef void   (APIENTRY *PFNDrawElements)(unsigned int mode, int count, unsigned int type, const void* indices);
  typedef unsigned int (APIENTRY *PFNCreateShader)(unsigned int type);
  typedef void   (APIENTRY *PFNShaderSource)(unsigned int shader, int count, const char** string, const int* length);
  typedef void   (APIENTRY *PFNCompileShader)(unsigned int shader);
  typedef void   (APIENTRY *PFNGetShaderiv)(unsigned int shader, unsigned int pname, int* params);
  typedef void   (APIENTRY *PFNGetShaderInfoLog)(unsigned int shader, int bufSize, int* length, char* infoLog);
  typedef unsigned int (APIENTRY *PFNCreateProgram)(void);
  typedef void   (APIENTRY *PFNAttachShader)(unsigned int program, unsigned int shader);
  typedef void   (APIENTRY *PFNLinkProgram)(unsigned int program);
  typedef void   (APIENTRY *PFNGetProgramiv)(unsigned int program, unsigned int pname, int* params);
  typedef void   (APIENTRY *PFNGetProgramInfoLog)(unsigned int program, int bufSize, int* length, char* infoLog);
  typedef void   (APIENTRY *PFNDeleteShader)(unsigned int shader);
#endif

// ── Constants ──────────────────────────────────────────────────────────────
#define GL_COLOR_BUFFER_BIT   0x00004000
#define GL_DEPTH_BUFFER_BIT   0x00000100
#define GL_DEPTH_TEST         0x0B71
#define GL_LEQUAL             0x0203
#define GL_TRIANGLES          0x0004
#define GL_FLOAT              0x1406
#define GL_FALSE              0
#define GL_ARRAY_BUFFER       0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8B89
#define GL_STATIC_DRAW        0x88E4
#define GL_VERTEX_SHADER      0x8B31
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_COMPILE_STATUS     0x8B81
#define GL_LINK_STATUS        0x8B82
#define GL_INFO_LOG_LENGTH    0x8B84
#define GL_VERSION            0x1F02
#define GL_RENDERER           0x1F01

// ── Globals ────────────────────────────────────────────────────────────────
static FILE* g_log = nullptr;
static int   g_errors = 0;

static void log_msg(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (g_log) { fprintf(g_log, "%s\n", buf); fflush(g_log); }
    SDL_Log("%s", buf);
}

static void check_gl(const char* step) {
    // We can't call glGetError through pointer here yet, skip for now
    (void)step;
}

// ── Shader source ──────────────────────────────────────────────────────────
static const char* VERT_SRC = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FRAG_SRC = R"(
#version 130
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
}
)";

// ── Cube data ──────────────────────────────────────────────────────────────
static const float CUBE_VERTS[] = {
    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
    -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
     0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
};
static const unsigned short CUBE_IDX[] = {
     0, 1, 2,  0, 2, 3,   4, 5, 6,  4, 6, 7,
     8, 9,10,  8,10,11,  12,13,14, 12,14,15,
    16,17,18, 16,18,19,  20,21,22, 20,22,23,
};

// ── MVP matrix (perspective * lookAt) ──────────────────────────────────────
static void make_mvp(float* m) {
    // Simple perspective
    float fov = 45.0f * 3.14159f / 180.0f;
    float aspect = 800.0f / 600.0f;
    float near = 0.1f, far = 100.0f;
    float f = 1.0f / tanf(fov * 0.5f);
    float rangeInv = 1.0f / (near - far);

    float proj[16] = {};
    proj[0] = f / aspect;
    proj[5] = f;
    proj[10] = (far + near) * rangeInv;
    proj[11] = -1.0f;
    proj[14] = 2.0f * far * near * rangeInv;

    // LookAt: eye(0,2,5) center(0,0,0) up(0,1,0)
    float ex=0, ey=2, ez=5;
    float cx=0, cy=0, cz=0;
    float fx=cx-ex, fy=cy-ey, fz=cz-ez;
    float fLen = sqrtf(fx*fx+fy*fy+fz*fz);
    fx/=fLen; fy/=fLen; fz/=fLen;
    float ux=0, uy=1, uz=0;
    float sx=fy*uz-fz*uy, sy=fz*ux-fx*uz, sz=fx*uy-fy*ux;
    float sLen = sqrtf(sx*sx+sy*sy+sz*sz);
    sx/=sLen; sy/=sLen; sz/=sLen;
    ux=sy*fz-sz*fy; uy=sz*fx-sx*fz; uz=sx*fy-sy*fx;

    float view[16] = {};
    view[0]=sx;  view[1]=ux;  view[2]=-fx;  view[3]=0;
    view[4]=sy;  view[5]=uy;  view[6]=-fy;  view[7]=0;
    view[8]=sz;  view[9]=uz;  view[10]=-fz; view[11]=0;
    view[12]=-(sx*ex+sy*ey+sz*ez);
    view[13]=-(ux*ex+uy*ey+uz*ez);
    view[14]=-(fx*ex+fy*ey+fz*ez);
    view[15]=1;

    // MVP = proj * view
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += proj[k*4+r] * view[c*4+k];
            m[c*4+r] = s;
        }
}

// ── Main ───────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    SDL_SetMainReady();

    g_log = fopen("C:\\Users\\barbo\\My project\\voxel_engine\\test_output.txt", "w");
    if (!g_log) { SDL_Log("Cannot open log file!"); return 1; }
    log_msg("=== TEST RENDER START ===");

    // ── 1. Init SDL ────────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        log_msg("FAIL: SDL_Init: %s", SDL_GetError());
        return 1;
    }
    log_msg("OK: SDL_Init");

    // ── 2. Set GL attributes ──────────────────────────────────────────
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    log_msg("OK: GL attributes set");

    // ── 3. Create window ──────────────────────────────────────────────
    SDL_Window* win = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!win) {
        log_msg("FAIL: SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit(); return 1;
    }
    log_msg("OK: Window created");

    // ── 4. Create GL context ──────────────────────────────────────────
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        log_msg("FAIL: SDL_GL_CreateContext: %s", SDL_GetError());
        SDL_DestroyWindow(win); SDL_Quit(); return 1;
    }
    SDL_GL_MakeCurrent(win, ctx);
    log_msg("OK: GL context created");

    // ── 5. Load GL functions ──────────────────────────────────────────
    #define LOAD(name) (void*)SDL_GL_GetProcAddress(#name)

    PFNClear            glClear           = (PFNClear)LOAD(glClear);
    PFNClearColor       glClearColor      = (PFNClearColor)LOAD(glClearColor);
    PFNEnable           glEnable          = (PFNEnable)LOAD(glEnable);
    PFNDepthFunc        glDepthFunc       = (PFNDepthFunc)LOAD(glDepthFunc);
    PFNViewport         glViewport        = (PFNViewport)LOAD(glViewport);
    PFNGetString        glGetString       = (PFNGetString)LOAD(glGetString);
    PFNGetError         glGetError        = (PFNGetError)LOAD(glGetError);
    PFNGenBuffers       glGenBuffers      = (PFNGenBuffers)LOAD(glGenBuffers);
    PFNBindBuffer       glBindBuffer      = (PFNBindBuffer)LOAD(glBindBuffer);
    PFNBufferData       glBufferData      = (PFNBufferData)LOAD(glBufferData);
    PFNGenVertexArrays  glGenVertexArrays = (PFNGenVertexArrays)LOAD(glGenVertexArrays);
    PFNBindVertexArray  glBindVertexArray = (PFNBindVertexArray)LOAD(glBindVertexArray);
    PFNEnableVertexAttribArray glEnableVertexAttribArray = (PFNEnableVertexAttribArray)LOAD(glEnableVertexAttribArray);
    PFNVertexAttribPointer glVertexAttribPointer = (PFNVertexAttribPointer)LOAD(glVertexAttribPointer);
    PFNGetAttribLocation glGetAttribLocation = (PFNGetAttribLocation)LOAD(glGetAttribLocation);
    PFNGetUniformLocation glGetUniformLocation = (PFNGetUniformLocation)LOAD(glGetUniformLocation);
    PFNUniformMatrix4fv glUniformMatrix4fv = (PFNUniformMatrix4fv)LOAD(glUniformMatrix4fv);
    PFNUniform4f        glUniform4f       = (PFNUniform4f)LOAD(glUniform4f);
    PFNUseProgram       glUseProgram      = (PFNUseProgram)LOAD(glUseProgram);
    PFNDrawElements     glDrawElements    = (PFNDrawElements)LOAD(glDrawElements);
    PFNCreateShader     glCreateShader    = (PFNCreateShader)LOAD(glCreateShader);
    PFNShaderSource     glShaderSource    = (PFNShaderSource)LOAD(glShaderSource);
    PFNCompileShader    glCompileShader   = (PFNCompileShader)LOAD(glCompileShader);
    PFNGetShaderiv      glGetShaderiv     = (PFNGetShaderiv)LOAD(glGetShaderiv);
    PFNGetShaderInfoLog glGetShaderInfoLog = (PFNGetShaderInfoLog)LOAD(glGetShaderInfoLog);
    PFNCreateProgram    glCreateProgram   = (PFNCreateProgram)LOAD(glCreateProgram);
    PFNAttachShader     glAttachShader    = (PFNAttachShader)LOAD(glAttachShader);
    PFNLinkProgram      glLinkProgram     = (PFNLinkProgram)LOAD(glLinkProgram);
    PFNGetProgramiv     glGetProgramiv    = (PFNGetProgramiv)LOAD(glGetProgramiv);
    PFNGetProgramInfoLog glGetProgramInfoLog = (PFNGetProgramInfoLog)LOAD(glGetProgramInfoLog);
    PFNDeleteShader     glDeleteShader    = (PFNDeleteShader)LOAD(glDeleteShader);

    #undef LOAD

    // Check critical ones
    int missing = 0;
    #define CHECK(ptr, name) if (!ptr) { log_msg("MISSING: %s", name); missing++; }
    CHECK(glClear, "glClear")
    CHECK(glClearColor, "glClearColor")
    CHECK(glEnable, "glEnable")
    CHECK(glDepthFunc, "glDepthFunc")
    CHECK(glGenBuffers, "glGenBuffers")
    CHECK(glBindBuffer, "glBindBuffer")
    CHECK(glBufferData, "glBufferData")
    CHECK(glGenVertexArrays, "glGenVertexArrays")
    CHECK(glBindVertexArray, "glBindVertexArray")
    CHECK(glEnableVertexAttribArray, "glEnableVertexAttribArray")
    CHECK(glVertexAttribPointer, "glVertexAttribPointer")
    CHECK(glGetAttribLocation, "glGetAttribLocation")
    CHECK(glGetUniformLocation, "glGetUniformLocation")
    CHECK(glUniformMatrix4fv, "glUniformMatrix4fv")
    CHECK(glUniform4f, "glUniform4f")
    CHECK(glUseProgram, "glUseProgram")
    CHECK(glDrawElements, "glDrawElements")
    CHECK(glCreateShader, "glCreateShader")
    CHECK(glCreateProgram, "glCreateProgram")
    CHECK(glLinkProgram, "glLinkProgram")
    #undef CHECK

    if (missing > 0) {
        log_msg("FAIL: %d GL functions missing", missing);
        fclose(g_log);
        SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
        return 1;
    }
    log_msg("OK: All %d GL functions loaded", 20);

    // ── 6. Print GL info ──────────────────────────────────────────────
    log_msg("GL_VERSION: %s", glGetString(GL_VERSION));
    log_msg("GL_RENDERER: %s", glGetString(GL_RENDERER));

    // ── 7. GL state ───────────────────────────────────────────────────
    glClearColor(0.1f, 0.15f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    log_msg("OK: GL state set");

    // ── 8. Compile vertex shader ──────────────────────────────────────
    GLuint vs = glCreateShader(0x8B31); // GL_VERTEX_SHADER
    glShaderSource(vs, 1, &VERT_SRC, nullptr);
    glCompileShader(vs);
    int compiled = 0;
    glGetShaderiv(vs, 0x8B81, &compiled); // GL_COMPILE_STATUS
    if (!compiled) {
        char log[1024]; int len = 0;
        glGetShaderInfoLog(vs, 1024, &len, log);
        log_msg("FAIL: Vertex shader: %s", log);
    } else {
        log_msg("OK: Vertex shader compiled");
    }

    // ── 9. Compile fragment shader ────────────────────────────────────
    GLuint fs = glCreateShader(0x8B30); // GL_FRAGMENT_SHADER
    glShaderSource(fs, 1, &FRAG_SRC, nullptr);
    glCompileShader(fs);
    compiled = 0;
    glGetShaderiv(fs, 0x8B81, &compiled);
    if (!compiled) {
        char log[1024]; int len = 0;
        glGetShaderInfoLog(fs, 1024, &len, log);
        log_msg("FAIL: Fragment shader: %s", log);
    } else {
        log_msg("OK: Fragment shader compiled");
    }

    // ── 10. Link program ──────────────────────────────────────────────
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int linked = 0;
    glGetProgramiv(prog, 0x8B82, &linked); // GL_LINK_STATUS
    if (!linked) {
        char log[1024]; int len = 0;
        glGetProgramInfoLog(prog, 1024, &len, log);
        log_msg("FAIL: Program link: %s", log);
    } else {
        log_msg("OK: Program linked (id=%d)", prog);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    // ── 11. Get locations ─────────────────────────────────────────────
    int locMVP  = glGetUniformLocation(prog, "uMVP");
    int locColor = glGetUniformLocation(prog, "uColor");
    int locPos   = glGetAttribLocation(prog, "aPos");
    log_msg("Locations: uMVP=%d uColor=%d aPos=%d", locMVP, locColor, locPos);

    if (locPos < 0) {
        log_msg("FAIL: aPos not found in shader!");
        fclose(g_log);
        SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
        return 1;
    }

    // ── 12. Create VAO ────────────────────────────────────────────────
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    log_msg("OK: VAO created (id=%d)", vao);

    // ── 13. Create VBO ────────────────────────────────────────────────
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(0x8892, vbo); // GL_ARRAY_BUFFER
    glBufferData(0x8892, sizeof(CUBE_VERTS), CUBE_VERTS, 0x88E4); // GL_STATIC_DRAW
    log_msg("OK: VBO created (id=%d, %d bytes)", vbo, (int)sizeof(CUBE_VERTS));

    // ── 14. Create IBO ────────────────────────────────────────────────
    GLuint ibo = 0;
    glGenBuffers(1, &ibo);
    glBindBuffer(0x8B89, ibo); // GL_ELEMENT_ARRAY_BUFFER
    glBufferData(0x8B89, sizeof(CUBE_IDX), CUBE_IDX, 0x88E4);
    log_msg("OK: IBO created (id=%d, %d bytes)", ibo, (int)sizeof(CUBE_IDX));

    // ── 15. Setup vertex attrib ───────────────────────────────────────
    glEnableVertexAttribArray(locPos);
    glVertexAttribPointer(locPos, 3, 0x1406, 0, 3 * sizeof(float), (void*)0); // GL_FLOAT
    log_msg("OK: Vertex attrib set (location=%d)", locPos);

    // ── 16. Compute MVP ──────────────────────────────────────────────
    float mvp[16];
    make_mvp(mvp);
    log_msg("MVP[0]=%.2f MVP[5]=%.2f MVP[10]=%.2f MVP[15]=%.2f", mvp[0], mvp[5], mvp[10], mvp[15]);

    // ── 17. First render (test) ───────────────────────────────────────
    glViewport(0, 0, 800, 600);
    glClear(0x00004000 | 0x00000100); // COLOR | DEPTH
    glUseProgram(prog);
    glUniformMatrix4fv(locMVP, 1, 0, mvp);
    glUniform4f(locColor, 1.0f, 1.0f, 1.0f, 1.0f);
    glDrawElements(0x0004, 36, 0x1403, (void*)0); // GL_TRIANGLES, GL_UNSIGNED_SHORT

    int err = glGetError();
    log_msg("After first draw: GL error = 0x%X", err);

    SDL_GL_SwapWindow(win);
    log_msg("OK: First frame rendered");

    // ── 18. Main loop ─────────────────────────────────────────────────
    log_msg("Entering main loop...");
    bool running = true;
    int frame = 0;
    while (running && frame < 300) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        glViewport(0, 0, 800, 600);
        glClear(0x00004000 | 0x00000100);

        glUseProgram(prog);
        glUniformMatrix4fv(locMVP, 1, 0, mvp);
        glUniform4f(locColor, 1.0f, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(vao);
        glBindBuffer(0x8892, vbo);
        glVertexAttribPointer(locPos, 3, 0x1406, 0, 3 * sizeof(float), (void*)0);
        glBindBuffer(0x8B89, ibo);
        glDrawElements(0x0004, 36, 0x1403, (void*)0);

        SDL_GL_SwapWindow(win);
        frame++;

        if (frame == 1) {
            int e = glGetError();
            log_msg("Frame 1: GL error = 0x%X", e);
        }
    }

    log_msg("Done. %d frames rendered.", frame);
    log_msg("=== TEST RENDER END ===");
    fclose(g_log);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
