#include "filter.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>

#else
#include <GL/glew.h>
#endif

#include <stdarg.h>
#include <stdio.h>

// If it's Android, use the Android logging system
#ifdef __ANDROID__
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, g_LogTag, __VA_ARGS__)
static char                 g_LogTag[] = "ImGuiExample";
#else
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#define DISABLE_CRT_CURVATURE

#if defined(IMGUI_IMPL_OPENGL_ES2)
static std::string glsl_version{"#version 100\nprecision highp float;"};
#else
static std::string glsl_version{"#version 130"};
#endif

static FILE* logfile = nullptr;
void TRACE(const char* fmt, ...) {
    if (!logfile) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    fflush(logfile);
    va_end(args);
}

static gl_functions *gl = nullptr;

class CrtEffect final {
    GLuint crt_shader{0};
    GLuint crt_vbo{0};
    GLuint locations[6]{0, 0, 0, 0, 0, 0};
    GLuint pos{0};

    int num_triangles{0};

public:
    CrtEffect(int window_width, int window_height);
    ~CrtEffect();

    void draw(int fbo_texture, int fbo_width, int fbo_height);
};

static CrtEffect* crt_effect = nullptr;

static std::string vs_src = R"(
    uniform float width;
    uniform float height;
    attribute vec2 position;
    varying vec2 uv;

    vec2 warp(vec2 position) {
        vec2 delta = position*0.5;
        // Warp the gl_Position to get the CRT curvature effect
        float delta2 = dot(delta.xy, delta.xy);
        float delta4 = delta2 * delta2;
        float warp_factor = 0.25;
        float delta_offset = delta4 * warp_factor;

        return (uv - delta * delta_offset)*2.0 - 1.0;
    }

    void main() {
        uv = position * 0.5 + 0.5; // + vec2(0.5/width, 0.5/height);
        gl_Position = vec4(position, 0.0, 1.0);
        //gl_Position = vec4(warp(position), 0.0, 1.0);
        //uv = vec2(0.999, 0.999);
    }
)";

// Inspired by https://www.gamedeveloper.com/programming/shader-tutorial-crt-emulation
static std::string fs_src = R"(
    uniform sampler2D tex;
    varying vec2 uv;
    uniform float width;
    uniform float height;
    uniform float scanline;

    uniform float offset[5]; // = float[](0.0, 1.0, 2.0, 3.0, 4.0);
    uniform float weight[5]; // = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
    // float offset[4] = float[](0.0, 1.0, 2.0, 3.0);
    // float weight[4] =    float[](0.2514970059, 0.2095808383, 0.1197604790, 0.0449101796);
    int num_weights = 5;

    vec2 warp(vec2 uv) {
        // Warp the gl_Position to get the CRT curvature effect
        vec2 delta = uv - 0.5;
        float delta2 = dot(delta.xy, delta.xy);
        float delta4 = delta2 * delta2;
        float warp_factor = 0.25;
        float delta_offset = delta4 * warp_factor;

        return uv + delta * delta_offset;
    }

    vec4 phosphor(sampler2D tex, vec2 uv, vec2 resolution) {
        //uv = warp(uv);
        vec2 fpx = uv * resolution;
        // Compute the fragment color
        vec4 color = 1.25*texture2D(tex, uv);
        // scanline
        if (floor(fpx.y) == scanline) {
            color = color * 1.5 / 1.25;
        }
        // CRT Effect
        // If the x coordinate is in the first 1/3rd of a pixel,
        // keep the red component only. In the middle 1/3rd, keep
        // the green component only. In the last 1/3rd, keep the
        // blue component only.
        // First, compute the fractional pixel value in the range [0, 1)
        // Using fmod
        fpx = mod(fpx, 3.0)/3.0;
        vec4 bal = vec4(1, 0.9, 1, 1.0);
        vec4 f = 0.3 * bal;
        if (fpx.x < 0.33) {
            f.r = bal.r;
        } else if (fpx.x < 0.66) {
            f.g = bal.g;
        } else {
            f.b = bal.b;
        }
        color = color * f;
        // Add scanlines, remember y is flipped
        if (fpx.y < 0.33) {
            color = color * 0.75;
        }
        return color;
    }

    vec4 blur(sampler2D image, vec2 uv, vec2 resolution, float radius) {
        vec4 color = 2.0*phosphor(image, uv, resolution) * weight[0];
        for (int i = 1; i < num_weights; i++) {
            color += phosphor(image, uv + vec2(offset[i] * radius / resolution.x, 0.0), resolution) * weight[i];
            color += phosphor(image, uv - vec2(offset[i] * radius / resolution.x, 0.0), resolution) * weight[i];
        }
        return color;
    }

    void main() {
        // Debug uv coords
        //gl_FragColor = vec4(uv, 0.0, 1.0);
        // Fractional pixel value
        //vec2 px = gl_FragCoord.xy;
        //gl_FragColor = texture2D(tex, px / vec2(width, height));
        // The problem with gl_FragCoord is it's not interpolated.
        // We need fractional pixel values.
        // We can get that by passing the position from the vertex shader
        // and interpolating it here.
        //gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        gl_FragColor = texture2D(tex, uv);
        //gl_FragColor = phosphor(tex, uv, vec2(width, height));
        //gl_FragColor = blur(tex, uv, vec2(width, height), 1.0);
    }
)";

CrtEffect::CrtEffect(int window_width, int window_height)
{
    if (window_width < 16) {
        window_width = 16;
    }
    if (window_height < 16) {
        window_height = 16;
    }
    TRACE("   CrtEffect::CrtEffect(%d, %d)\n", window_width, window_height);

    // To get us started, use a simple blit shader to copy the contents of the FBO to the window
    TRACE("   create vertex shader\n");

    crt_shader = gl->CreateProgram();
    GLuint vs = gl->CreateShader(GL_VERTEX_SHADER);
    std::string src = glsl_version + "\n" + vs_src;
    const char *src_cstr = src.c_str();
    gl->ShaderSource(vs, 1, &src_cstr, NULL);
    gl->CompileShader(vs);
    GLint isCompiled = 0;
    gl->GetShaderiv(vs, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        gl->GetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        gl->GetShaderInfoLog(vs, maxLength, &maxLength, &errorLog[0]);

        // Provide the infolog in whatever manor you deem best.
        LOGE("Vertex shader compilation failed: %s\n", errorLog.data());
        TRACE("   Vertex shader compilation failed: %s\n", errorLog.data());

        // Exit with failure.
        gl->DeleteShader(vs); // Don't leak the shader.
        return;
    }
    gl->AttachShader(crt_shader, vs);

    TRACE("   create fragment shader\n");
    GLuint fs = gl->CreateShader(GL_FRAGMENT_SHADER);
    src = glsl_version + "\n" + fs_src;
    src_cstr = src.c_str();
    gl->ShaderSource(fs, 1, &src_cstr, NULL);
    gl->CompileShader(fs);
    gl->GetShaderiv(fs, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        gl->GetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        gl->GetShaderInfoLog(fs, maxLength, &maxLength, &errorLog[0]);

        // Provide the infolog in whatever manor you deem best.
        LOGE("Fragment shader compilation failed: %s\n", errorLog.data());
        TRACE("   Fragment shader compilation failed: %s\n", errorLog.data());

        // Exit with failure.
        gl->DeleteShader(fs); // Don't leak the shader.
        return;
    }
    gl->AttachShader(crt_shader, fs);

    TRACE("   link shader\n");
    gl->LinkProgram(crt_shader);

    // Check that all went well
    GLint status;
    gl->GetProgramiv(crt_shader, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        gl->GetProgramiv(crt_shader, GL_INFO_LOG_LENGTH, &log_length);
        char* log = new char[log_length];
        gl->GetProgramInfoLog(crt_shader, log_length, NULL, log);
        LOGE("Link error: %s\n", log);
        TRACE("   Link error: %s\n", log);
        delete[] log;
    }

    int i = 0;
    locations[i++] = gl->GetUniformLocation(crt_shader, "tex");
    locations[i++] = gl->GetUniformLocation(crt_shader, "width");
    locations[i++] = gl->GetUniformLocation(crt_shader, "height");
    locations[i++] = gl->GetUniformLocation(crt_shader, "scanline");
    locations[i++] = gl->GetUniformLocation(crt_shader, "offset");
    locations[i++] = gl->GetUniformLocation(crt_shader, "weight");

    pos = gl->GetAttribLocation(crt_shader, "position");

    TRACE("   create vertex array object\n");
#ifdef DISABLE_CRT_CURVATURE
    gl->GenBuffers(1, &crt_vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, crt_vbo);
    float vertices[] = {
        -1.0f,
        -1.0f,
        1.0f,
        -1.0f,
        1.0f,
        1.0f,
        -1.0f,
        1.0f,
    };
    gl->BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
#else
    // Generate a mesh of 20x20 pixel big tiles that will be used to apply the CRT shader
    // the mesh will be deformed to emulate CRT curvature
    gl->GenBuffers(1, &crt_vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, crt_vbo);

    int numTilesX = window_width / 20; // 20x20 pixel tiles
    int numTilesY = window_height / 20;
    float tileWidth = 2.0f / numTilesX;
    float tileHeight = 2.0f / numTilesY;

    std::vector<float> vertices;
    for (int y = 0; y < numTilesY; y++) {
        for (int x = 0; x < numTilesX; x++) {
            float startX = -1.0f + x * tileWidth;
            float startY = -1.0f + y * tileHeight;
            float endX = startX + tileWidth;
            float endY = startY + tileHeight;

            // Triangle 1
            vertices.push_back(startX);
            vertices.push_back(startY);
            vertices.push_back(endX);
            vertices.push_back(startY);
            vertices.push_back(endX);
            vertices.push_back(endY);

            // Triangle 2
            vertices.push_back(startX);
            vertices.push_back(startY);
            vertices.push_back(endX);
            vertices.push_back(endY);
            vertices.push_back(startX);
            vertices.push_back(endY);
        }
    }
    //assert(vertices.size() == numTilesX * numTilesY * 6 * 2);
    num_triangles = vertices.size() / 6;
    gl->BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
#endif
    // See if we got any GL errors in the queue
    GLenum err;
    while ((err = gl->GetError()) != GL_NO_ERROR) {
        LOGE("GL error: 0x%x\n", err);
        TRACE("   GL error: 0x%x\n", err);
    }

    TRACE("   set uniforms\n");
    //uniform float offset[5]; // = float[](0.0, 1.0, 2.0, 3.0, 4.0);
    //uniform float weight[5]; // = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
    // Set the offset and weight uniforms
    float offset[5] = {0.0, 1.0, 2.0, 3.0, 4.0};
    float weight[5] = {0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162};
    gl->UseProgram(crt_shader);
    gl->Uniform1fv(locations[4], 5, offset);
    gl->Uniform1fv(locations[5], 5, weight);
    gl->UseProgram(0);

    TRACE("   exit CrtEffect::CrtEffect\n");
}

CrtEffect::~CrtEffect()
{
    TRACE("   CrtEffect::~CrtEffect\n");
    gl->DeleteProgram(crt_shader);
    gl->DeleteBuffers(1, &crt_vbo);
    TRACE("   exit CrtEffect::~CrtEffect\n");
}

static std::atomic_int scanline;

void CrtEffect::draw(int fbo_texture, int fbo_width, int fbo_height)
{
    TRACE("   CrtEffect::draw(%d, %d, %d)\n", fbo_texture, fbo_width, fbo_height);

    // Render the FBO to the window using the CRT shader
    TRACE("      glActiveTexture\n");
    gl->ActiveTexture(GL_TEXTURE0);
    TRACE("      glBindTexture\n");
    gl->BindTexture(GL_TEXTURE_2D, fbo_texture);
    TRACE("      glUseProgram\n");
    gl->UseProgram(crt_shader);
    TRACE("      glUniform1i\n");
    gl->Uniform1i(locations[0], 0);
    TRACE("      glUniform1f\n");
    gl->Uniform1f(locations[1], fbo_width);
    TRACE("      glUniform1f\n");
    gl->Uniform1f(locations[2], fbo_height);

    TRACE("      glUniform1f\n");
    gl->Uniform1f(locations[3], static_cast<float>(scanline % fbo_height));

    TRACE("      glBindBuffer\n");
    gl->BindBuffer(GL_ARRAY_BUFFER, crt_vbo);
    TRACE("      glVertexAttribPointer\n");
    gl->VertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);
    TRACE("      glEnableVertexAttribArray\n");
    gl->EnableVertexAttribArray(pos);

    TRACE("      glDrawArrays\n");
#ifdef DISABLE_CRT_CURVATURE
    gl->DrawArrays(GL_TRIANGLE_FAN, 0, 4);
#else
    //glDrawArrays(/*GL_TRIANGLES*/ GL_LINE_STRIP, 0, num_triangles * 3);
    gl->DrawArrays(GL_TRIANGLES, 0, num_triangles * 3);
#endif

    TRACE("      glDisableVertexAttribArray\n");
    gl->DisableVertexAttribArray(pos);
    TRACE("      glBindBuffer\n");
    gl->BindBuffer(GL_ARRAY_BUFFER, 0);
    TRACE("      glUseProgram\n");
    gl->UseProgram(0);

    TRACE("   exit CrtEffect::draw\n");
}

void filter_init(const char *logfile_path, struct gl_functions *gl_interface)
{
    gl = gl_interface;
    if (logfile_path && !logfile) {
        logfile = fopen(logfile_path, "w");
        if (!logfile) {
            LOGE("Failed to open log file %s\n", logfile_path);
        }
    }
    TRACE("filter_init\n");
    if (crt_effect) {
        TRACE("early return filter_init\n");

        return;
    }

    // Get the viewport from OpenGL state
    GLint viewport[4];
    gl->GetIntegerv(GL_VIEWPORT, viewport);
    crt_effect = new CrtEffect(viewport[2], viewport[3]);
    TRACE("exit filter_init\n");
}

void filter_gl_context_lost()
{
    delete crt_effect;
    crt_effect = nullptr;
}

void filter_gl_context_restored()
{
    TRACE("filter_gl_context_restored\n");
    if (crt_effect) {
        TRACE("early return filter_gl_context_restored\n");

        return;
    }
    // Get the viewport from OpenGL state
    GLint viewport[4];
    gl->GetIntegerv(GL_VIEWPORT, viewport);
    crt_effect = new CrtEffect(viewport[2], viewport[3]);
    TRACE("exit filter_gl_context_restored\n");
}

void filter_draw(int fbo_texture, int width, int height)
{
    TRACE("filter_draw\n");
    if (!crt_effect) {
        TRACE("early return filter_draw\n");
        return;
    }

    crt_effect->draw(fbo_texture, width, height);
    TRACE("exit filter_draw\n");
}

void filter_register_callback(void (*callback)())
{
    TRACE("filter_register_callback\n");
    // Spawn a thread that will call the callback function 30 times per second
    // This is used to animate the CRT effect
    std::thread t([callback]() {
        scanline = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
            ++scanline;
            callback();
        }
    });
    t.detach();
    TRACE("exit filter_register_callback\n");
}
