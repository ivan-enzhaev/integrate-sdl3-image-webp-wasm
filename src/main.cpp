#define SDL_MAIN_USE_CALLBACKS 1
#include <iostream>
#include <cstdio>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <GLES3/gl3.h>

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

static GLuint createTexture(const char *path)
{
    // Use SDL_IOFromFile to correctly read the file from preloaded virtual filesystem
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io)
    {
        printf("Failed to open file stream for %s: %s\n", path, SDL_GetError());
        return 0;
    }

    // Load the image from the SDL stream
    SDL_Surface *surface = IMG_Load_IO(io, 1); // 1 (true) automatically closes and frees the SDL_IOStream
    if (!surface)
    {
        printf("Image loading failed: %s\n", SDL_GetError());
        return 0;
    }

    // Convert to RGBA / ABGR8888 for WebGL 2.0
    SDL_Surface *optimizedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);

    if (!optimizedSurface)
    {
        printf("Surface conversion failed: %s\n", SDL_GetError());
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic"))
    {
        float maxAnisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

        // Set the quality to maximum (usually 8.0 or 16.0)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

        printf("Anisotropic filtering enabled. Max: %.1f\n", maxAnisotropy);
    }
    else
    {
        printf("Anisotropic filtering not supported.\n");
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, optimizedSurface->w, optimizedSurface->h,
        0, GL_RGBA, GL_UNSIGNED_BYTE, optimizedSurface->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    SDL_DestroySurface(optimizedSurface);

    return textureID;
}

// WebGL 2.0 / OpenGL ES 3.0 Shaders
static const char* vertexShaderSource = R"(#version 300 es
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* fragmentShaderSource = R"(#version 300 es
precision mediump float;

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

void main() {
    FragColor = texture(ourTexture, TexCoord);
}
)";

struct AppState {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    GLuint texture = 0;
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
};

// Called once when the application starts
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Initialize SDL3 video subsystem
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState* state = new AppState();
    *appstate = state;

    // Configure WebGL 2.0 / OpenGL ES 3.0 context attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create SDL window mapped to #canvas
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    state->window = SDL_CreateWindow("SDL3_image WebP WebGL2", 1280, 720, window_flags);
    if (!state->window)
    {
        printf("Failed to create SDL window: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create WebGL 2.0 context
    state->gl_context = SDL_GL_CreateContext(state->window);
    if (!state->gl_context)
    {
        printf("Failed to create WebGL 2.0 context: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(state->window, state->gl_context);
    SDL_GL_SetSwapInterval(1); // Enable V-Sync

    // Shaders & Geometry
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    state->shaderProgram = glCreateProgram();
    glAttachShader(state->shaderProgram, vertexShader);
    glAttachShader(state->shaderProgram, fragmentShader);
    glLinkProgram(state->shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    float vertices[] = {
        // Positions   // TexCoords
        -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  1.0f, 1.0f,

        -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &state->VAO);
    glGenBuffers(1, &state->VBO);

    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Load preloaded WebP texture from virtual filesystem
    state->texture = createTexture("assets/textures/clear_01.webp");

    printf("Application initialized successfully\n");
    return SDL_APP_CONTINUE;
}

// Called for every incoming SDL event
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

// Called once per frame to update and render
SDL_AppResult SDL_AppIterate(void* appstate)
{
    AppState* state = (AppState*)appstate;

    int display_w, display_h;
    SDL_GetWindowSizeInPixels(state->window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClearColor(0.2f, 0.25f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (state->texture)
    {
        glUseProgram(state->shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state->texture);
        glBindVertexArray(state->VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    SDL_GL_SwapWindow(state->window);

    return SDL_APP_CONTINUE;
}

// Called once when the application is shutting down
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    (void)result;
    AppState* state = (AppState*)appstate;

    if (state)
    {
        if (state->texture) glDeleteTextures(1, &state->texture);
        if (state->VAO) glDeleteVertexArrays(1, &state->VAO);
        if (state->VBO) glDeleteBuffers(1, &state->VBO);
        if (state->shaderProgram) glDeleteProgram(state->shaderProgram);

        if (state->gl_context)
        {
            SDL_GL_DestroyContext(state->gl_context);
        }
        if (state->window)
        {
            SDL_DestroyWindow(state->window);
        }

        delete state;
    }

    SDL_Quit();
}
