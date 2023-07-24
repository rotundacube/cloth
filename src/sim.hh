#ifndef SIM_HH
#define SIM_HH

#include "vec.hh"
#include <vector>

struct Particle
{
    Vec2 pos;
    Vec2 acc;
    Vec2 old_pos;
    float mass;
    bool fixed;                          

    void update(float dt);
};

struct Constraint
{
    Particle *a, *b;
    float min_dist, max_dist;

    void apply();
};

struct Rope
{
    std::vector<Particle> points;
    std::vector<Constraint> constraints;
    
    Rope(Vec2 start, Vec2 end, int count);

    void update(float dt);
};

struct Cloth
{
    std::vector<Particle> points;
    std::vector<Constraint> constraints;
    int width, height;
    Vec2 size;

    Cloth() {}
    Cloth(Vec2 start, Vec2 size, int w, int hs);

    void update(float dt);
};

#endif // SIM_HH
