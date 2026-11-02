#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Rect {
    float x;
    float y;
    float width;
    float height;
};

struct CoreColor {
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
};

#endif // TYPES_H
