#pragma once

extern "C" {
struct gl_functions {
    unsigned int (*CreateProgram)();
    unsigned int (*CreateShader)(unsigned int type);
    void (*ShaderSource)(unsigned int shader, int count, const char *const *string, const int *length);
    void (*CompileShader)(unsigned int shader);
    void (*AttachShader)(unsigned int program, unsigned int shader);
    void (*LinkProgram)(unsigned int program);
    void (*GetShaderiv)(unsigned int shader, unsigned int pname, int *params);
    void (*GetProgramiv)(unsigned int program, unsigned int pname, int *params);
    void (*GetShaderInfoLog)(unsigned int shader, int bufSize, int *length, char *infoLog);
    void (*GetProgramInfoLog)(unsigned int program, int bufSize, int *length, char *infoLog);
    int (*GetUniformLocation)(unsigned int program, const char *name);
    int (*GetAttribLocation)(unsigned int program, const char *name);
    void (*DeleteShader)(unsigned int shader);
    void (*DeleteProgram)(unsigned int program);
    void (*UseProgram)(unsigned int program);
    void (*GenBuffers)(int n, unsigned int *buffers);
    void (*DeleteBuffers)(int n, const unsigned int *buffers);
    void (*BindBuffer)(unsigned int target, unsigned int buffer);
    void (*BufferData)(unsigned int target, long size, const void *data, unsigned int usage);
    unsigned int (*GetError)();
    void (*Uniform1fv)(int location, int count, const float *value);
    void (*Uniform1f)(int location, float v0);
    void (*Uniform1i)(int location, int v0);
    void (*GetIntegerv)(unsigned int pname, int *params);
    void (*Viewport)(int x, int y, int width, int height);
    void (*ClearColor)(float red, float green, float blue, float alpha);
    void (*Clear)(unsigned int mask);
    void (*BindFramebuffer)(unsigned int target, unsigned int framebuffer);
    void (*GenFramebuffers)(int n, unsigned int *framebuffers);
    void (*GenTextures)(int n, unsigned int *textures);
    void (*BindTexture)(unsigned int target, unsigned int texture);
    void (*ActiveTexture)(unsigned int texture);
    void (*VertexAttribPointer)(unsigned int index, int size, unsigned int type, unsigned char normalized, int stride, const void *pointer);
    void (*EnableVertexAttribArray)(unsigned int index);
    void (*DrawArrays)(unsigned int mode, int first, int count);
    void (*DisableVertexAttribArray)(unsigned int index);
    void (*TexImage2D)(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels);
    void (*TexParameteri)(unsigned int target, unsigned int pname, int param);
    void (*FramebufferTexture2D)(unsigned int target, unsigned int attachment, unsigned int textarget, unsigned int texture, int level);
    unsigned int (*CheckFramebufferStatus)(unsigned int target);
    void (*DeleteFramebuffers)(int n, const unsigned int *framebuffers);
    void (*DeleteTextures)(int n, const unsigned int *textures);
};

void filter_init(const char *logfile, struct gl_functions *gl);
void filter_gl_context_lost();
void filter_gl_context_restored();
void filter_draw(int tex, int width, int height);
// Called when filter needs to be redrawn
void filter_register_callback(void (*callback)());
}
