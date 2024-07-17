#include <blurgtext.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <glad/glad.h>

typedef struct {
    float M[4][4];
} mat4;

static void ortho(mat4* mat, float left, float right, float bottom, float top)
{
    mat->M[0][0] = 2. / (right - left);
	mat->M[1][1] = 2. / (top - bottom);
	mat->M[2][2] = - 1.;
	mat->M[3][0] = - (right + left) / (right - left);
	mat->M[3][1] = - (top + bottom) / (top - bottom);
    mat->M[3][3] = 1.;
}

static void tallocate(blurg_texture_t *texture, int width, int height)
{
    uint32_t tex;
    glGenTextures(1, &tex);
    texture->userdata = (void*)(uintptr_t)tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

static void tupdate(blurg_texture_t *texture, void *buffer, int x, int y, int width, int height)
{
    uint32_t tex = (uint32_t)(uintptr_t)texture->userdata;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
}

typedef struct vertex {
    float x;
    float y;
    float u;
    float v;
} vertex_t;

const char *vertex_shader = "#version 140\nin vec2 pos; in vec2 uv; out vec2 texcoord; uniform mat4 matrix; void main() { texcoord = uv; gl_Position = matrix * vec4(pos, 0.0, 1.0); }";
const char *fragment_shader = "#version 140\nin vec2 texcoord; uniform sampler2D tex; out vec4 col; void main() { col = texture(tex, texcoord); }";

static void printShaderInfoLog(uint32_t handle, const char *type)
{
    int status, log_length;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if(!status)
        printf("compile failed for %s\n", type);
    if(log_length > 0) {
        char *log = malloc(log_length + 1);
        glGetShaderInfoLog(handle, log_length, NULL, log);
        printf("%s\n", log);
        free(log);
    }
}

static uint32_t vbo;

static void drawRects(blurg_rect_t *rects, int count, int x, int y)
{
    vertex_t *vertices = malloc(sizeof(vertex_t) * count * 6);
    vertex_t *vptr = vertices;
    for(int i = 0; i < count; i++) {
        vertex_t top_left = {
            .x = x + rects[i].x,
            .y = y + rects[i].y,
            .u = rects[i].u0,
            .v = rects[i].v0,
        };
        vertex_t top_right = {
            .x = x + rects[i].x + rects[i].width,
            .y = y + rects[i].y,
            .u = rects[i].u1,
            .v = rects[i].v0
        };
        vertex_t bottom_left = {
            .x = x + rects[i].x,
            .y = y + rects[i].y + rects[i].height,
            .u = rects[i].u0,
            .v = rects[i].v1
        };
        vertex_t bottom_right = {
            .x = x + rects[i].x + rects[i].width,
            .y = y + rects[i].y + rects[i].height,
            .u = rects[i].u1,
            .v = rects[i].v1
        };
        *vptr++ = top_left;
        *vptr++ = top_right;
        *vptr++ = bottom_left;
        *vptr++ = top_right;
        *vptr++ = bottom_right;
        *vptr++ = bottom_left;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_t) * count * 6, vertices, GL_STATIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, count * 6);
    free(vertices);
}

static void drawString(blurg_t* blurg, blurg_font_t *font, const char *text, int x, int y)
{
    int count;
    blurg_rect_t *rects = blurg_build_string(blurg, font, 36.0, text, &count);
    drawRects(rects, count, x, y);
    blurg_free_rects(rects);
}

static blurg_font_t* loadFont(blurg_t *blurg, const char *filename)
{
    const char *basePath = SDL_GetBasePath();
    char path[1000];
    snprintf(path, 1000, "%s/%s", basePath, filename);
    return blurg_font_create(blurg, path);
}

int main(int argc, char* argv[])
{
    SDL_Window *Window = SDL_CreateWindow("OpenGL Test", 0, 0, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext Context = SDL_GL_CreateContext(Window);
    gladLoadGLLoader(SDL_GL_GetProcAddress);
    
    // Generate string
    blurg_t *blurg = blurg_create(tallocate, tupdate);
    blurg_font_t *font = loadFont(blurg, "Roboto-Regular.ttf");

    uint32_t vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof(vertex_t), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(vertex_t), (void*)(2 * sizeof(float)));

    uint32_t vtxsh = glCreateShader(GL_VERTEX_SHADER);
    int shlen = strlen(vertex_shader);
    glShaderSource(vtxsh, 1, &vertex_shader, &shlen);
    uint32_t fgsh = glCreateShader(GL_FRAGMENT_SHADER);
    shlen = strlen(fragment_shader);
    glShaderSource(fgsh, 1, &fragment_shader, &shlen);
    glCompileShader(vtxsh);
    printShaderInfoLog(vtxsh, "vertex");
    glCompileShader(fgsh);
    printShaderInfoLog(fgsh, "fragment");
    uint32_t program = glCreateProgram();
    glAttachShader(program, vtxsh);
    glAttachShader(program, fgsh);
    glBindAttribLocation(program, 0, "pos");
    glBindAttribLocation(program, 1, "uv");
    glLinkProgram(program);
    int matLocation = glGetUniformLocation(program, "matrix");

  int Running = 1;
  while (Running)
  {
    SDL_Event Event;
    while (SDL_PollEvent(&Event))
    {
      if (Event.type == SDL_KEYDOWN)
      {
        switch (Event.key.keysym.sym)
        {
          case SDLK_ESCAPE:
            Running = 0;
            break;
          default:
            break;
        }
      }
      else if (Event.type == SDL_QUIT)
      {
        Running = 0;
      }
    }

    int winWidth, winHeight;

    SDL_GL_GetDrawableSize(Window, &winWidth, &winHeight);
    glViewport(0, 0, winWidth, winHeight);
    glClearColor(0., 0., 0., 1.);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    mat4 matrix;
    memset(&matrix, 0, sizeof(mat4));
    ortho(&matrix, 0, winWidth, winHeight, 0);
    glUniformMatrix4fv(matLocation, 1, 0, (GLfloat*)&matrix);
    glUniform1i(glGetUniformLocation(program, "tex"), 0);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    drawString(blurg, font, "Hello World!\nNewline test", 8, 8);

    blurg_style_span_t spans[2];
    memset(spans, 0, sizeof(blurg_style_span_t) * 2);
    spans[0].startIndex = 5;
    spans[0].endIndex = 8;
    spans[0].font = font;
    spans[0].fontSize = 90.0;
    spans[1].startIndex = 14;
    spans[1].endIndex = 16;
    spans[1].font = font;
    spans[1].fontSize = 24.0;

    blurg_formatted_text_t formatted = {
        .text = "Style sp\nan tes\nt 12345",
        .defaultFont = font,
        .defaultSize = 36.0,
        .spans = spans,
        .spanCount = 2,
        .alignment = blurg_align_center
    };

    int fcount;
    blurg_rect_t* formattedRects = blurg_build_formatted(blurg, &formatted, &fcount);
    drawRects(formattedRects, fcount, 8, 200);
    blurg_free_rects(formattedRects);

    SDL_GL_SwapWindow(Window);
  }
}