// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#else
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <vector>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

//#define DISABLE_CRT_CURVATURE

int upscale_x = 3;
int upscale_y = 6;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // We want to render ImGUI content to an offscreen FBO of size 320x240.
    // Then we draw the contents of the FBO to screen using a CRT shader. We need at least 4x upscale here
    // int fbo_size[2] = { 320, 240 };
    // int upscale = 4;
    // We need an upscale of a multiple of 3 to get the correct CRT shader effect
    int fbo_size[2] = {640, 240};
    int window_size[2] = {fbo_size[0] * upscale_x, fbo_size[1] * upscale_y};
    bool use_crt_shader = true;

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(window_size[0], window_size[1], "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(2); // Enable vsync. Setting interval to 1 fails to give 60 Hz on my machine, use 30 Hz instead.

    glewInit();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    float scale = use_crt_shader ? 1.0f : upscale_x;
    ImGui::GetStyle().ScaleAllSizes(scale); // Scale UI

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("Topaz_a1200_v1.0.ttf", 16.0f * scale);
    io.Fonts->AddFontFromFileTTF("Topaz_a500_v1.0.ttf", 16.0f * scale);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_implot_demo = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // If GLFW reports that key F11 was pressed, toggle fullscreen
        static bool toggle = false;
        if (!toggle && glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
            toggle = true;

            static int w = window_size[0], h = window_size[1];
            if (glfwGetWindowMonitor(window)) {
                glfwSetWindowMonitor(window, nullptr, 100, 100, w, h, 0);
                glfwSetWindowSize(window, w, h);
            } else {
                // Get the size when windowed
                glfwGetWindowSize(window, &w, &h);
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }

        if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_RELEASE) {
            toggle = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        if (show_implot_demo)
            ImPlot::ShowDemoWindow(&show_implot_demo);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin("Another Window", &show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        if (use_crt_shader) {
            static GLuint fbo = 0;
            static GLuint fbo_texture = 0;
            static GLuint crt_shader = 0;
            static GLuint crt_texture = 0;
            static GLuint crt_vao = 0;
            static GLuint crt_vbo = 0;
            static GLuint locations[4] = {0, 0, 0, 0};
            if (!fbo) {
                glGenFramebuffers(1, &fbo);
                glGenTextures(1, &fbo_texture);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glBindTexture(GL_TEXTURE_2D, fbo_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_size[0], fbo_size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                // Make sure we get black color when uv coordinate is out of bounds
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    fprintf(stderr, "Framebuffer not complete\n");
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // To get us started, use a simple blit shader to copy the contents of the FBO to the window
                crt_shader = glCreateProgram();
                GLuint vs = glCreateShader(GL_VERTEX_SHADER);
                const char* vs_src = R"(
                    #version 130
                    uniform float width;
                    uniform float height;
                    in vec2 position;
                    out vec2 uv;

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
                glShaderSource(vs, 1, &vs_src, NULL);
                glCompileShader(vs);
                glAttachShader(crt_shader, vs);
                GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
                // Inspired by https://www.gamedeveloper.com/programming/shader-tutorial-crt-emulation
                const char* fs_src = R"(
                    #version 130
                    uniform sampler2D tex;
                    in vec2 uv;
                    out vec4 frag_color;
                    uniform float width;
                    uniform float height;
                    uniform float scanline;

                    //uniform float offset[5] = float[](0.0, 1.0, 2.0, 3.0, 4.0);
                    //uniform float weight[5] = float[](0.2270270270, 0.1945945946, 0.1216216216,
                    //                                   0.0540540541, 0.0162162162);
                    uniform float offset[4] = float[](0.0, 1.0, 2.0, 3.0);
                    uniform float weight[4] =    float[](0.2514970059, 0.2095808383, 0.1197604790,
                                                         0.0449101796);
                    uniform int num_weights = 4;

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
                        vec4 color = 1.25*texture(tex, uv);
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
                        fpx = 1.0*(fpx - floor(fpx));
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
                        vec4 color = 2*phosphor(image, uv, resolution) * weight[0];
                        for (int i = 1; i < num_weights; i++) {
                            color += phosphor(image, uv + vec2(offset[i] * radius / resolution.x, 0.0), resolution) * weight[i];
                            color += phosphor(image, uv - vec2(offset[i] * radius / resolution.x, 0.0), resolution) * weight[i];
                        }
                        return color;
                    }

                    void main() {
                        // Debug uv coords
                        //frag_color = vec4(uv, 0.0, 1.0);
                        // Fractional pixel value
                        //vec2 px = gl_FragCoord.xy;
                        //frag_color = texture(tex, px / vec2(width, height));
                        // The problem with gl_FragCoord is it's not interpolated.
                        // We need fractional pixel values.
                        // We can get that by passing the position from the vertex shader
                        // and interpolating it here.
                        //frag_color = texture(tex, uv);
                        //frag_color = phosphor(tex, uv, vec2(width, height));
                        frag_color = blur(tex, uv, vec2(width, height), 1.0);
                    }
                )";
                glShaderSource(fs, 1, &fs_src, NULL);
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
            }

            ImGui::Render();

            // Make the FBO current and draw all the IMGUI stuff to it
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, fbo_size[0], fbo_size[1]);
            glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            // The viewport used for glClear will not be used by ImGUI, it will apply its own state to GL and restore our state before returning
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Prepare the window surface
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            // Center the FBO in the window and scale proportionally to fit the window
            float s = std::min((float)display_w / window_size[0], (float)display_h / window_size[1]);
            int x = (display_w - s*window_size[0]) / 2;
            int y = (display_h - s*window_size[1]) / 2;
            glViewport(x, y, s*window_size[0], s*window_size[1]);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            // Render the FBO to the window using the CRT shader
            if (!crt_texture) {
                glGenTextures(1, &crt_texture);
                glBindTexture(GL_TEXTURE_2D, crt_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_size[0], fbo_size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fbo_texture);
            glUseProgram(crt_shader);
            glUniform1i(locations[0], 0);
            glUniform1f(locations[1], fbo_size[0]);
            glUniform1f(locations[2], fbo_size[1]);
            static float scanline = 0;
            ++scanline;
            if (scanline == fbo_size[1])
                scanline = 0;
            glUniform1f(locations[3], scanline);
#ifndef DISABLE_CRT_CURVATURE
            static int num_triangles = 0;
#endif
            if (!crt_vao) {
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

                int numTilesX = window_size[0] / 20; // 20x20 pixel tiles
                int numTilesY = window_size[1] / 20;
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
                assert(vertices.size() == numTilesX * numTilesY * 6 * 2);
                num_triangles = vertices.size() / 6;
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
#endif
                // See if we got any GL errors in the queue
                GLenum err;
                while ((err = glGetError()) != GL_NO_ERROR) {
                    fprintf(stderr, "GL error: %d\n", err);
                }
            }

            glBindVertexArray(crt_vao);

#ifdef DISABLE_CRT_CURVATURE
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#else
            //glDrawArrays(/*GL_TRIANGLES*/ GL_LINE_STRIP, 0, num_triangles * 3);
            glDrawArrays(GL_TRIANGLES, 0, num_triangles * 3);
#endif
        } else {
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
