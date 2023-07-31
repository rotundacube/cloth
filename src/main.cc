#ifdef EMSCRIPTEN
#include <SDL.h>
#include <SDL_opengles2.h>
#include <emscripten.h>
#else
#define GLEW_STATIC
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif

#include "sim.hh"
#include <stdlib.h>

static int g_width;
static int g_height;
static Vec2 g_aspect;

static Vec2 screen_to_point(Vec2 p)
{
    return 
        p.map({0, (float)g_height}, 
              {(float)g_width, 0},
              -g_aspect, g_aspect);
}

// NOTE: shaders should be simple enough to where no error detection is needed
static GLuint compile_shaders(char const *vs, char const *fs)
{
    char message[512] = {0};

    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vs, nullptr);
    glCompileShader(vertex);

    GLint result, log_length;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertex, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0)
    {
        glGetShaderInfoLog(vertex, sizeof(message) - 1, nullptr, message);
        printf("%s\n", message);
    }

    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fs, nullptr);
    glCompileShader(fragment);
    
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragment, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0)
    {
        glGetShaderInfoLog(fragment, sizeof(message) - 1, nullptr, message);
        printf("%s\n", message);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

#ifndef EMSCRIPTEN
    glBindFragDataLocation(program, 0, "fragColor");
#endif

    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

struct ClothRender
{
    Cloth sim;
    GLuint sim_vbo;
    GLuint sim_ebo;
    GLuint sim_shader;
    GLuint sim_texture;

    GLint sim_uv_attrib;
    GLint sim_pos_attrib;
    GLint sim_aspect_loc;
    float sim_accumlator = 0;

    struct Vertex
    {
        float xy[2];
        float uv[2];
    };

    std::vector<Vertex> vertex_data;
    std::vector<uint32_t> index_data;

    GLuint point_vbo;
    GLuint point_shader;
    GLint point_uv_attrib;
    GLint point_pos_attrib;
    GLint point_aspect_loc;
    std::vector<Vertex> point_vertex_data;
    static constexpr float POINT_RADIUS = 0.05;
    
    Vec2 mouse_delta = {};
    Particle *held_particle = nullptr;
    
    ClothRender()
    {

#ifdef EMSCRIPTEN
#define VS_PREFIX            \
    "#define in attribute\n" \
    "#define out varying\n"
#define FS_PREFIX                                       \
    "#extension GL_OES_standard_derivatives : enable\n" \
    "#define in varying\n"                              \
    "#define fragColor gl_FragColor\n"                  \
    "precision highp float;\n"
#else
#define VS_PREFIX \
    "#version 150\n"
#define FS_PREFIX                 \
    "#version 150\n"              \
    "#define texture2D texture\n" \
    "out vec4 fragColor;\n"
#endif

        constexpr char sim_vs[] =
VS_PREFIX
R"(
in vec2 uv;
in vec2 pos;
out vec2 out_uv;
uniform vec2 aspect;
void main()
{
    out_uv = uv;
    gl_Position = vec4(pos/aspect, 0., 1.);
})";

        constexpr char sim_fs[] =
FS_PREFIX
R"(
in vec2 out_uv;
uniform sampler2D tex2d;
void main()
{
    fragColor = texture2D(tex2d, out_uv);
})";

        constexpr char point_vs[] =
VS_PREFIX
R"(
in vec2 uv;
in vec2 pos;
out vec2 tex;
uniform vec2 aspect;
void main()
{
    tex = uv;
    gl_Position = vec4(pos/aspect, 0., 1.);
})";

        constexpr char point_fs[] =
FS_PREFIX
R"(
in vec2 tex;
void main()
{
    float s = min(dFdx(tex.x), dFdy(tex.y));
    float c = smoothstep(s/2., 0., length(tex-.5)-.5);
    fragColor = vec4(0, 0, 1, c);
})";

#undef VS_PREFIX
#undef FS_PREFIX

        sim_shader = compile_shaders(sim_vs, sim_fs); 

#ifndef EMSCRIPTEN
        GLuint sim_vao;
        glGenVertexArrays(1, &sim_vao);
        glBindVertexArray(sim_vao);
#endif 

        glGenBuffers(1, &sim_ebo);
        glGenBuffers(1, &sim_vbo);

        constexpr uint8_t image_data[4][4] = {
            {0, 0, 0, 255}, {255, 0, 0, 255},
            {0, 255, 0, 255}, {255, 255, 0, 255},
        };

        glGenTextures(1, &sim_texture);
        update_texture_data(&image_data[0][0], 2, 2);
        generate_indices();

        sim_uv_attrib = glGetAttribLocation(sim_shader, "uv");
        sim_pos_attrib = glGetAttribLocation(sim_shader, "pos");
        sim_aspect_loc = glGetUniformLocation(sim_shader, "aspect");

        point_shader = compile_shaders(point_vs, point_fs);

#ifndef EMSCRIPTEN
        GLuint point_vao;
        glGenVertexArrays(1, &point_vao);
        glBindVertexArray(point_vao);
#endif

        glGenBuffers(1, &point_vbo);

        point_uv_attrib = glGetAttribLocation(point_shader, "uv");
        point_pos_attrib = glGetAttribLocation(point_shader, "pos");
        point_aspect_loc = glGetUniformLocation(point_shader, "aspect");
    }

    void update_texture_data(uint8_t const *data, 
                             int width, int height)
    {
        glBindTexture(GL_TEXTURE_2D, sim_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        float aspect = float(height)/float(width);
        sim = Cloth({-.75f, .75f}, aspect*Vec2{1.5f, 1.5f}, 50, 50);
        generate_vertices();
    }

    void generate_indices()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sim_ebo);
        index_data.resize(6*(sim.width - 1)*(sim.height - 1));
        for (int i = 0; i < sim.height - 1; ++i)
        {
            for (int j = 0; j < sim.width - 1; ++j)
            {
                int vertex = (j + i*(sim.width));
                int index = 6*(j + i*(sim.width - 1));
                
                // lower left triangle
                index_data[index + 0] = vertex;
                index_data[index + 1] = vertex + sim.width;
                index_data[index + 2] = vertex + sim.width + 1;
                
                // upper right triangle
                index_data[index + 3] = vertex;
                index_data[index + 4] = vertex + 1;
                index_data[index + 5] = vertex + sim.width + 1;      
            }
        }

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                     index_data.size()*
                     sizeof index_data[0],
                     index_data.data(),
                     GL_DYNAMIC_DRAW);
    }

    void generate_vertices()
    {
        glBindBuffer(GL_ARRAY_BUFFER, sim_vbo);
        vertex_data.resize(sim.width*sim.height);
        for (int i = 0; i < sim.height; ++i)
        {
            for (int j = 0; j < sim.width; ++j)
            {
                int index = j + i*sim.width;
                vertex_data[index] = {
                    {sim.points[index].pos.x, sim.points[index].pos.y},
                    {j/float(sim.width - 1), 1 - i/float(sim.height - 1)},
                };
            }
        }

        glBufferData(GL_ARRAY_BUFFER, 
                     vertex_data.size() * 
                     sizeof vertex_data[0],
                     vertex_data.data(), 
                     GL_DYNAMIC_DRAW);                
    }

    void update_points()
    {
        point_vertex_data.clear();

        float min_dist = -1;
        Particle *nearest = nullptr;

        int x, y;
        Uint32 button = SDL_GetMouseState(&x, &y);

        Vec2 mouse = screen_to_point(Vec2{(float)x, (float)y});    
        for (auto &p : sim.points)
        {
            if (!p.fixed) continue;

            // add a quad to draw this point
            Vec2 q = p.pos;
            float size = POINT_RADIUS;
            point_vertex_data.insert(point_vertex_data.end(),
                                     {
                                        // upper right
                                        {{q.x - size, q.y + size}, {0, 1}},
                                        {{q.x + size, q.y + size}, {1, 1}},
                                        {{q.x + size, q.y - size}, {1, 0}},
                                        
                                        // lower left
                                        {{q.x - size, q.y + size}, {0, 1}},
                                        {{q.x - size, q.y - size}, {0, 0}},
                                        {{q.x + size, q.y - size}, {1, 0}},
                                     });

            float d = q.dist(mouse);
            if (d < POINT_RADIUS && 
                (min_dist < 0 || d < min_dist))
            {
                nearest = &p;
                min_dist = d;
            }
        }

        if ((button & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0)
        {
            if (nearest != nullptr && held_particle == nullptr)
            {
                held_particle = nearest;
                mouse_delta = held_particle->pos - mouse;
            }

            if (held_particle != nullptr) 
            {
                held_particle->pos += 
                    (mouse + mouse_delta - 
                     held_particle->pos)*0.25f;
            }
        }
        else
        {
            held_particle = nullptr;
        }

        glBindBuffer(GL_ARRAY_BUFFER, point_vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                     point_vertex_data.size() * 
                     sizeof point_vertex_data[0],
                     point_vertex_data.data(), 
                     GL_DYNAMIC_DRAW);
    }

    void update(float dt)
    {
        update_points();

        constexpr float min_dt = 1/60.0f;
        sim_accumlator += fminf(dt, 0.25f);
        while (sim_accumlator >= min_dt)
        {
            sim.update(min_dt);
            sim_accumlator -= min_dt;
        }

        generate_vertices();
    }

    // NOTE: webgl 1 doesn't have vao
    void draw()
    {
        glUseProgram(point_shader);

        glBindBuffer(GL_ARRAY_BUFFER, point_vbo);
        glEnableVertexAttribArray(point_uv_attrib);
        glEnableVertexAttribArray(point_pos_attrib);
        
        glVertexAttribPointer(point_uv_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, uv)));
        
        glVertexAttribPointer(point_pos_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, xy)));

        glUniform2f(point_aspect_loc, g_aspect.x, g_aspect.y);
        glDrawArrays(GL_TRIANGLES, 0, point_vertex_data.size());

        glUseProgram(sim_shader);

        glBindBuffer(GL_ARRAY_BUFFER, sim_vbo);
        glEnableVertexAttribArray(sim_uv_attrib);
        glEnableVertexAttribArray(sim_pos_attrib);
        
        glVertexAttribPointer(sim_uv_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, uv)));
        
        glVertexAttribPointer(sim_pos_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, xy)));

        glBindTexture(GL_TEXTURE_2D, sim_texture);
        glUniform2f(sim_aspect_loc, g_aspect.x, g_aspect.y);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sim_ebo);
        glDrawElements(GL_TRIANGLES, 
                       index_data.size(),
                       GL_UNSIGNED_INT, 
                       nullptr);
    }
};

struct LoopData
{
    Uint64 now;
    Uint64 last;
    Uint64 freq;

    SDL_Window *window;
    ClothRender simulation;

    static void update_stub(void *self)
    {
        ((LoopData*)self)->update();
    }

    void update()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                exit(0);
            }
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        if (w != g_width || h != g_height)
        {
            g_width = w;
            g_height = h;
            glViewport(0, 0, w, h);

            g_aspect = 
                Vec2{(float)w, (float)h} / 
                Vec2{(float)h, (float)w};
            
            if (w > h)
            {
                g_aspect.y = 1;
            }

            if (h > w)
            {
                g_aspect.x = 1;
            }
        }

        last = now;
        now = SDL_GetPerformanceCounter();
        float frame_time = (float)(double(now - last)/double(freq));
        simulation.update(frame_time);

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(1, 1, 1, 1);
        simulation.draw();
        
        SDL_GL_SwapWindow(window);
    }
};

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    
#ifdef EMSCRIPTEN
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    SDL_Window *window = 
        SDL_CreateWindow("Cloth Simulation", 
                         SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED,
                         640, 480, 
                         SDL_WINDOW_OPENGL | 
                         SDL_WINDOW_SHOWN  |
                         SDL_WINDOW_RESIZABLE);

    SDL_GL_SetSwapInterval(0);
    SDL_GL_CreateContext(window);

#ifndef EMSCRIPTEN
    if (glewInit() != GLEW_OK)
    {
        return -1;
    }
#endif

    Uint64 last = 0;
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    LoopData loop_data = {};
    loop_data.now = now;
    loop_data.last = last;
    loop_data.freq = freq;
    loop_data.window = window;

    // enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef EMSCRIPTEN
    for (;;)
    {
        loop_data.update();
    }
#else
    emscripten_set_main_loop_arg(&LoopData::update_stub, &loop_data, 0, 1);
#endif

    return 0;
}
