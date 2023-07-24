#define GLEW_STATIC
#include <GL/glew.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "sim.hh"

constexpr int G_WIDTH = 640;
constexpr int G_HEIGHT = 640;
constexpr float G_ASPECT = G_WIDTH/float(G_HEIGHT);

static Vec2 point_to_screen(Vec2 p)
{
    return 
        p.map({-G_ASPECT, -1}, 
              {G_ASPECT, 1},
              {0, G_HEIGHT}, 
              {G_WIDTH, 0});
}

static Vec2 screen_to_point(Vec2 p)
{
    return 
        p.map({0, G_HEIGHT}, 
              {G_WIDTH, 0},
              {-G_ASPECT, -1}, 
              {G_ASPECT, 1});
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

    glBindFragDataLocation(program, 0, "fragColor");
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

struct ClothRender
{
    Cloth sim;
    GLuint vbo;
    GLuint ebo;
    float sim_accumlator = 0;

    GLuint shader;
    GLint uv_attrib;
    GLint pos_attrib;

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
    std::vector<Vertex> point_vertex_data;
    static constexpr float POINT_RADIUS = 5;
    
    Vec2 mouse_delta = {};
    Particle *held = nullptr;
    
    ClothRender() :
        sim({-.75f, .75f}, {1.5f, 1.5f}, 50, 50)
    {
        constexpr char sim_vs[] =  R"(
#version 150
in vec2 uv;
in vec2 pos;
out vec2 tex;
void main()
{
    tex = uv;
    gl_Position = vec4(pos.xy, 0., 1.);
})";

        constexpr char sim_fs[] =  R"(
#version 150
in vec2 tex;
out vec4 fragColor;
void main()
{
    float c = smoothstep(.009, 0, length(tex-.5)-.5);
    fragColor = vec4(mix(vec3(.5), vec3(tex, 0.), c), 1.);
})";

        constexpr char point_vs[] =  R"(
#version 150
in vec2 uv;
in vec2 pos;
out vec2 tex;
void main()
{
    tex = uv;
    gl_Position = vec4(pos.xy, 0.0, 1.0);
})";

        constexpr char point_fs[] =  R"(
#version 150
in vec2 tex;
out vec4 fragColor;
void main()
{
    float c = smoothstep(.009, 0, length(tex-.5)-.5);
    fragColor = vec4(0, 0, c, c);
})";

        shader = compile_shaders(sim_vs, sim_fs); 

        GLuint sim_vao;
        glGenVertexArrays(1, &sim_vao);
        glBindVertexArray(sim_vao);

        glGenBuffers(1, &ebo);
        generate_indices();

        glGenBuffers(1, &vbo);
        generate_vertices();

        uv_attrib = glGetAttribLocation(shader, "uv");
        pos_attrib = glGetAttribLocation(shader, "pos");

        point_shader = compile_shaders(point_vs, point_fs);

        GLuint point_vao;
        glGenVertexArrays(1, &point_vao);
        glBindVertexArray(point_vao);

        glGenBuffers(1, &point_vbo);

        point_uv_attrib =
            glGetAttribLocation(point_shader, "uv");

        point_pos_attrib =
            glGetAttribLocation(point_shader, "pos");   
    }

    void generate_indices()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
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
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        vertex_data.resize(sim.width*sim.height);
        for (int i = 0; i < sim.height; ++i)
        {
            for (int j = 0; j < sim.width; ++j)
            {
                int index = j + i*sim.width;
                vertex_data[index] = {
                    sim.points[index].pos.x,
                    sim.points[index].pos.y,
                    j/float(sim.width - 1), 
                    1 - i/float(sim.height - 1),
                };
            }
        }

        glBufferData(GL_ARRAY_BUFFER, 
                     vertex_data.size() * 
                     sizeof vertex_data[0],
                     vertex_data.data(), 
                     GL_DYNAMIC_DRAW);                
    }

    void update(float dt)
    {
        point_vertex_data.clear();

        float min_dist = -1;
        Particle *nearest = nullptr;

        int x, y;
        Uint32 button = SDL_GetMouseState(&x, &y);

        Vec2 real_mouse = {(float)x, (float)y};
        Vec2 mouse = screen_to_point(real_mouse);    
        for (auto &p : sim.points)
        {
            if (!p.fixed) continue;

            // add a quad to draw this point
            Vec2 q = p.pos;
            float size = 2*POINT_RADIUS/(float)G_HEIGHT;
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

            Vec2 s = point_to_screen(p.pos);
            float d = s.dist(real_mouse);
            if (d < POINT_RADIUS && 
                (min_dist < 0 || d < min_dist))
            {
                nearest = &p;
                min_dist = d;
            }
        }

        if ((button & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0)
        {
            if (nearest != nullptr && held == nullptr)
            {
                held = nearest;
                mouse_delta = held->pos - mouse;
            }

            if (held != nullptr) 
            {
                held->pos += 
                    (mouse + mouse_delta - held->pos)*0.25f;
            }
        }
        else
        {
            held = nullptr;
        }

        glBindBuffer(GL_ARRAY_BUFFER, point_vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                     point_vertex_data.size() * 
                     sizeof point_vertex_data[0],
                     point_vertex_data.data(), 
                     GL_DYNAMIC_DRAW);

        constexpr float min_dt = 1/60.0f;
        sim_accumlator += fminf(dt, 0.25f);
        while (sim_accumlator >= min_dt)
        {
            sim.update(min_dt);
            sim_accumlator -= min_dt;
        }

        generate_vertices();
    }

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

        glDrawArrays(GL_TRIANGLES, 0, point_vertex_data.size());

        glUseProgram(shader);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(uv_attrib);
        glEnableVertexAttribArray(pos_attrib);
        
        glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, uv)));
        
        glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, 
                              GL_FALSE, sizeof(Vertex), 
                              (void *)(offsetof(Vertex, xy)));
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glDrawElements(GL_TRIANGLES, 
                       index_data.size(),
                       GL_UNSIGNED_INT, 
                       nullptr);
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
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = 
        SDL_CreateWindow("", 
                         SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED,
                         G_WIDTH, G_HEIGHT, 
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(0);

    if (glewInit() != GLEW_OK)
    {
        return -1;
    }

    // enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // setup cloth
    ClothRender simulation;

    Uint64 last = 0;
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    
    for (;;)
    {
        SDL_Event e;
        if (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT) break;
            continue;
        }

        last = now;
        now = SDL_GetPerformanceCounter();
        float frame_time = (float)(double(now - last)/double(freq));
        
        simulation.update(frame_time);

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        simulation.draw();
        
        SDL_GL_SwapWindow(window);
    }

    return 0;
}
