#include "sim.hh"
#include "raylib.h"
#include "raymath.h"

#include <stdint.h>

constexpr int WIDTH = 600;
constexpr int HEIGHT = 600;
constexpr float ASPECT = WIDTH/float(HEIGHT);

static Vec2 point_to_screen(Vec2 p)
{
    return 
        p.map({-ASPECT, -1}, 
              {ASPECT, 1},
              {0, HEIGHT}, 
              {WIDTH, 0});
}

static Vec2 screen_to_point(Vec2 p)
{
    return 
        p.map({0, HEIGHT}, 
              {WIDTH, 0},
              {-ASPECT, -1}, 
              {ASPECT, 1});
}

static void draw_line(Vec2 a, Vec2 b)
{
    Vec2 p0 = point_to_screen(a);
    Vec2 p1 = point_to_screen(b);
    DrawLineEx({p0.x, p0.y}, {p1.x, p1.y}, 3, BLUE);
}

static void draw_sim(Rope &sim)
{
    for (size_t i = 0; i < sim.points.size() - 1; ++i)
    {
        draw_line(sim.points[i].pos, sim.points[i + 1].pos);   
    }
}

static void draw_sim(Cloth &sim)
{
    for (int i = 0; i < sim.height; ++i)
    {
        for (int j = 0; j < sim.width; ++j)
        {
            if (j + 1 < sim.width)
            {
                draw_line(sim.points[j + i*sim.width].pos, 
                          sim.points[j + 1 + i*sim.width].pos);
            }

            if (i + 1 < sim.height)
            {
                draw_line(sim.points[j + i*sim.width].pos, 
                          sim.points[j + (i + 1)*sim.width].pos);
            }   
        }
    } 
}

int main()
{
    InitWindow(WIDTH, HEIGHT, "");
    SetTargetFPS(144);

#if 0
    Rope sim({0, 0}, {0, -0.6}, 50);
#else
    Cloth sim({-.5, 0.3}, {1, 1}, 50, 50);
#endif

    Vec2 mouse_delta = {};
    Particle *held = nullptr;
    while (!WindowShouldClose())
    {
        BeginDrawing();
        {
            ClearBackground(RAYWHITE);

            // update sim
            float dt = GetFrameTime();
            sim.update(dt);

            // draw sim
            draw_sim(sim);

            // draw circles on fixed point
            // find the circle nearest to the mouse.
            constexpr float RADIUS = 10;

            float min_dist = -1;
            Particle *nearest = nullptr;
            Vector2 real_mouse = GetMousePosition();
            Vec2 mouse = screen_to_point({real_mouse.x, real_mouse.y});
            for (auto &p : sim.points)
            {
                if (!p.fixed) continue;

                Vec2 s = point_to_screen(p.pos);
                float d = s.dist({real_mouse.x, real_mouse.y});
                if (d < RADIUS && 
                    (min_dist < 0 || d < min_dist))
                {
                    nearest = &p;
                    min_dist = d;
                }

               //DrawCircle(s.x, s.y, RADIUS, RED);
            }

            if (IsMouseButtonDown(0))
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
        }
        EndDrawing();
    }

    CloseWindow();
}
