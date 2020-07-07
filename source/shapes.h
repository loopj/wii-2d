#ifndef __SHAPES_H__
#define __SHAPES_H__

#include <gccore.h>

typedef enum _drawMode {
    DRAW_MODE_FILL,
    DRAW_MODE_OUTLINE
} DrawMode;

typedef struct _color {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Color;

typedef struct _vertex {
    u32 x;
    u32 y;
} Vertex;

void setColor(Color color);
void setLineWidth(int width);

void drawPolygon(DrawMode mode, int count, Vertex *vertices, Mtx transform);
void drawLine(int count, Vertex *vertices, Mtx transform);
void drawRectangle(DrawMode mode, float x, float y, float width, float height, Mtx transform);
void drawEllipse(DrawMode mode, float x, float y, float radiusX, float radiusY, Mtx transform);

#endif