#include "sim.hh"
#include <stdio.h>

void Particle::update(float dt)
{
    if (fixed) {
        old_pos = pos;
        return;
    }
    
    Vec2 copy = pos;
    pos = 2*pos - old_pos + dt*dt*acc;
    old_pos = copy;
}

Rope::Rope(Vec2 start, Vec2 end, int count) :
    points(count + 2)
{
    Vec2 step = (end - start)/float(count + 1);
    float line_width = end.dist(start)/(count + 1);
    for (int i = 0; i < count + 2; ++i)
    {
        points[i] = {
            start,
            {0, -9.81},
            start,
            1,
            false,
        };
        
        start = start + step;
    }

    for (int j = 1; j <= 10; ++j)
    {
        for (int i = j; i < count + 2; ++i)
        {
            constraints.push_back({
                &points[i], 
                &points[i - j], 
                0, line_width*j
            });
        }
    }

    points[0].fixed = true;
}

void Rope::update(float dt)
{
    for (auto &p : points)
    {
        p.update(dt);
    }

    for (int j = 0; j < 30; ++j)
    {
        for (auto &c : constraints)
        {
            c.apply();
        }
    }
}

Cloth &Cloth::operator=(Cloth &&o) 
{
    size = o.size;
    width = o.width;
    height = o.height;
    points = std::move(o.points);
    constraints = std::move(o.constraints);
    return *this;
}

Cloth::Cloth(Vec2 start, Vec2 s, int w, int h) :
    points(w * h),
    width(w),
    height(h),
    size(s)
{
    Vec2 col = {(size/float(width - 1)).x, 0};
    Vec2 row = {0, (size/float(height - 1)).y};

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            Vec2 pos = start - row*float(i) + col*float(j);
            points[j + width*i] = {pos, {0, -9.81}, pos, 1, false}; 

        }
    }

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            if (j + 1 < width)
            {
                constraints.push_back({
                    &points[j + i*width], 
                    &points[j + 1 + i*width],
                    0, col.x,
                });       
            }

            if (i + 1 < height)
            {
                constraints.push_back({
                    &points[j + i*width], 
                    &points[j + (i + 1)*width],
                    0, row.y,
                });       
            }
        }
    }

    for (int j = 1; j <= width; ++j)
    {
        for (int i = j; i < width; ++i)
        {
            constraints.push_back({
                &points[i],
                &points[i - j],
                0, col.x*float(j),
            });
        }
    }
    
    for (int i = 0; i < width; ++i)
    {
        points[i].mass = 100.0f;
    }
    
    points[0].fixed = true;
    points[width - 1].fixed = true;
}

void Cloth::update(float dt)
{
    for (auto &p : points)
    {
        p.update(dt);
    }

    for (int j = 0; j < 30; ++j)
    {
        for (auto &c : constraints)
        {
            c.apply();
        }
    }
}

void Constraint::apply()
{                              
    float dist = a->pos.dist(b->pos);
    
    float error = 0;
    if (dist < min_dist)
    { 
        error = dist - min_dist;
    }
    else if (dist > max_dist) 
    {
        error = dist - max_dist;
    }

    // NOTE: both weights should sum to 1 or 0(both are fixed).
    float mass = a->mass + b->mass;

    float a_weight = 1 - a->mass/mass;
    float b_weight = 1 - b->mass/mass;

    if (a->fixed && b->fixed)
    {
        a_weight = 0;
        b_weight = 0;
    }
    else if (a->fixed)
    {
        a_weight = 0;
        b_weight = 1;
    } 
    else if (b->fixed)
    {
        b_weight = 0;
        a_weight = 1;
    }
    
    Vec2 delta = (a->pos - b->pos).normalize()*error;
    a->pos -= delta*a_weight;
    b->pos += delta*b_weight;
}
