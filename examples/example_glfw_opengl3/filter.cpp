#include "filter.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>

#define glBindVertexArray       glBindVertexArrayOES
#define glGenVertexArrays       glGenVertexArraysOES
#define glDeleteVertexArrays    glDeleteVertexArraysOES
#define GL_VERTEX_ARRAY_BINDING GL_VERTEX_ARRAY_BINDING_OES

#else
#include <GL/glew.h>
#endif

#include <stdio.h>

#define LOGE(...) fprintf(stderr, __VA_ARGS__)

#include <atomic>
#include <string>
#include <thread>
#include <vector>

//#define DISABLE_CRT_CURVATURE

#if defined(IMGUI_IMPL_OPENGL_ES2)
static std::string glsl_version{"#version 100\nprecision highp float;"};
#else
static std::string glsl_version{"#version 130"};
#endif

class CrtEffect {
    GLuint crt_shader{0};
    GLuint crt_vao{0};
    GLuint crt_vbo{0};
    GLuint locations[6]{0, 0, 0, 0, 0, 0};

    int num_triangles{0};

public:
    CrtEffect(int window_width, int window_height);

    void draw(int fbo_texture, int fbo_width, int fbo_height);
};

static CrtEffect* crt_effect = nullptr;

static std::string vs_src = R"(
    uniform float width;
    uniform float height;
    in vec2 position;
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
        //gl_Position = vec4(position, 0.0, 1.0);
        gl_Position = vec4(warp(position), 0.0, 1.0);
        //uv = vec2(0.999, 0.999);
    }
)";

// Inspired by https://www.gamedeveloper.com/programming/shader-tutorial-crt-emulation
static std::string fs_src = R"(
    uniform sampler2D tex;
    in vec2 uv;
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
        //gl_FragColor = texture2D(tex, uv);
        //gl_FragColor = phosphor(tex, uv, vec2(width, height));
        gl_FragColor = blur(tex, uv, vec2(width, height), 1.0);
    }
)";

CrtEffect::CrtEffect(int window_width, int window_height)
{
    // To get us started, use a simple blit shader to copy the contents of the FBO to the window
    crt_shader = glCreateProgram();
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    std::string src = glsl_version + "\n" + vs_src;
    const char *src_cstr = src.c_str();
    glShaderSource(vs, 1, &src_cstr, NULL);
    glCompileShader(vs);
    glAttachShader(crt_shader, vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    src = glsl_version + "\n" + fs_src;
    src_cstr = src.c_str();
    glShaderSource(fs, 1, &src_cstr, NULL);
    glCompileShader(fs);
    glAttachShader(crt_shader, fs);
    glLinkProgram(crt_shader);

    // Check that all went well
    GLint status;
    glGetProgramiv(crt_shader, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_length;
        glGetProgramiv(crt_shader, GL_INFO_LOG_LENGTH, &log_length);
        char* log = new char[log_length];
        glGetProgramInfoLog(crt_shader, log_length, NULL, log);
        fprintf(stderr, "Link error: %s\n", log);
        delete[] log;
    }

    int i = 0;
    locations[i++] = glGetUniformLocation(crt_shader, "tex");
    locations[i++] = glGetUniformLocation(crt_shader, "width");
    locations[i++] = glGetUniformLocation(crt_shader, "height");
    locations[i++] = glGetUniformLocation(crt_shader, "scanline");
    locations[i++] = glGetUniformLocation(crt_shader, "offset");
    locations[i++] = glGetUniformLocation(crt_shader, "weight");

#ifdef DISABLE_CRT_CURVATURE
    glGenVertexArrays(1, &crt_vao);
    glBindVertexArray(crt_vao);
    glGenBuffers(1, &crt_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, crt_vbo);
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
#else
    // Generate a mesh of 20x20 pixel big tiles that will be used to apply the CRT shader
    // the mesh will be deformed to emulate CRT curvature
    glGenVertexArrays(1, &crt_vao);
    glBindVertexArray(crt_vao);
    glGenBuffers(1, &crt_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, crt_vbo);

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
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
#endif
    // See if we got any GL errors in the queue
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        LOGE("GL error: %d\n", err);
    }

    //uniform float offset[5]; // = float[](0.0, 1.0, 2.0, 3.0, 4.0);
    //uniform float weight[5]; // = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
    // Set the offset and weight uniforms
    float offset[5] = {0.0, 1.0, 2.0, 3.0, 4.0};
    float weight[5] = {0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162};
    glUseProgram(crt_shader);
    glUniform1fv(locations[4], 5, offset);
    glUniform1fv(locations[5], 5, weight);
}

static std::atomic_int scanline;

void CrtEffect::draw(int fbo_texture, int fbo_width, int fbo_height)
{
    // Render the FBO to the window using the CRT shader
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glUseProgram(crt_shader);
    glUniform1i(locations[0], 0);
    glUniform1f(locations[1], fbo_width);
    glUniform1f(locations[2], fbo_height);

    glUniform1f(locations[3], static_cast<float>(scanline % fbo_height));

    glBindVertexArray(crt_vao);

#ifdef DISABLE_CRT_CURVATURE
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#else
    //glDrawArrays(/*GL_TRIANGLES*/ GL_LINE_STRIP, 0, num_triangles * 3);
    glDrawArrays(GL_TRIANGLES, 0, num_triangles * 3);
#endif
}

void filter_init()
{
    if (crt_effect)
        return;

    // Get the viewport from OpenGL state
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    crt_effect = new CrtEffect(viewport[2], viewport[3]);
}

void filter_draw(int fbo_texture, int width, int height)
{
    if (!crt_effect)
        return;

    crt_effect->draw(fbo_texture, width, height);
}

void filter_register_callback(void (*callback)())
{
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
}
