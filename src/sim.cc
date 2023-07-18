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
            {0, -29.81},
            start,
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
            points[j + height*i] = {pos, {0, -15.81}, pos, false}; 

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

    //for (int k = 0; k < 5; ++k)
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


        {
            points[0].fixed = false;
            points[width - 1].fixed = false;
            Constraint{&points[0], &points[width - 1], 0, 1.0f}.apply();
            points[0].fixed = true;
            points[width - 1].fixed = true;
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

    // NOTE: both weights should sum to 1.
    float a_weight = 0.5f;
    float b_weight = 0.5f;

    if (a->fixed)
    {
        a_weight = 0;
        b_weight *= 2;
    }

    if (b->fixed)
    {
        b_weight = 0;
        a_weight *= 2;
    }

    Vec2 delta = (a->pos - b->pos).normalize()*error;
    a->pos -= delta*a_weight;
    b->pos += delta*b_weight;
}
