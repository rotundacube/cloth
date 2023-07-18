#ifndef VEC_HH
#define VEC_HH

#include <math.h>

struct Vec2
{
    float x, y;

    inline friend Vec2 operator+(Vec2 a, Vec2 b)
    {
        return {a.x + b.x, a.y + b.y};
    }

    inline friend Vec2 operator-(Vec2 a, Vec2 b)
    {
        return {a.x - b.x, a.y - b.y};
    }

    inline friend Vec2 operator*(Vec2 a, Vec2 b)
    {
        return {a.x * b.x, a.y * b.y};
    }

    inline friend Vec2 operator/(Vec2 a, Vec2 b)
    {
        return {a.x / b.x, a.y / b.y};
    }

    inline friend Vec2 operator*(Vec2 a, float b)
    {
        return {a.x*b, a.y*b};
    }
    
    inline friend Vec2 operator*(float a, Vec2 b)
    {
        return b*a;
    }

    inline friend Vec2 operator/(Vec2 a, float b)
    {
        return {a.x/b, a.y/b};
    }
    
    inline friend Vec2 operator-(Vec2 a)
    {
        return {-a.x, -a.y};
    }

    Vec2 &operator +=(Vec2 b)
    {
        *this = *this + b;
        return *this;
    }

    Vec2 &operator -=(Vec2 b)
    {
        *this = *this - b;
        return *this;
    }
    
    float length() const
    {
        return sqrtf(x*x + y*y);
    }

    float dist(Vec2 o) const
    {
        return (*this - o).length();
    }

    Vec2 normalize() const
    {
        return *this/this->length();
    }

    Vec2 map(Vec2 in_min, Vec2 in_max, 
             Vec2 out_min, Vec2 out_max) const
    {
        Vec2 in = (*this - in_min)/(in_max - in_min);
        return in*(out_max - out_min) + out_min;
    }
};

#endif // VEC_HH
