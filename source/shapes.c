#include <math.h>
#include <gccore.h>

#include "shapes.h"

Color drawColor;

//
// Set the current draw color
//
void setColor(Color color) {
    drawColor = color;
}

//
// Set the width of a line
//
void setLineWidth(int width) {
    GX_SetLineWidth(width, GX_TO_ONE);
}

//
// Draw a filled or outlined polygon
//
void drawPolygon(DrawMode mode, int count, Vertex *vertices, Mtx transform) {
    Mtx model;
    guMtxIdentity(model);

    // Apply additional transforms (if any)
    if(transform) {
        guMtxConcat(model, transform, model);
    }

    // Load position data
    GX_LoadPosMtxImm(model, GX_PNMTX0);

    // Draw the polygon
    if(mode == DRAW_MODE_FILL) {
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, count);
        for(int i = 0; i < count; i++) {
            GX_Position2f32(vertices[i].x, vertices[i].y);
            GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);
        }
        GX_End();
    } else {
        GX_Begin(GX_LINESTRIP, GX_VTXFMT0, count + 1);
        for(int i = 0; i < count + 1; i++) {
            GX_Position2f32(vertices[i % count].x, vertices[i % count].y);
            GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);
        }
        GX_End();
    }
}

//
// Draw a line or a set of lines
//
void drawLine(int count, Vertex *vertices, Mtx transform) {
    Mtx model;
    guMtxIdentity(model);

    // Apply additional transforms (if any)
    if(transform) {
        guMtxConcat(model, transform, model);
    }

    // Load position data
    GX_LoadPosMtxImm(model, GX_PNMTX0);

    // Draw the lines
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, count);
    for(int i = 0; i < count; i++) {
        GX_Position2f32(vertices[i].x, vertices[i].y);
        GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);
    }
    GX_End();
}

//
// Draw a filled or outlined rectangle
//
void drawRectangle(DrawMode mode, float x, float y, float width, float height, Mtx transform) {
    // Move to x/y location
    Mtx temp;
    guMtxIdentity(temp);
    guMtxTrans(temp, x, y, 0);
    if(transform) {
        guMtxConcat(transform, transform, temp);
    } else {
        transform = temp;
    }

    drawPolygon(mode, 4, (Vertex[4]){
        {0,     0},
        {width, 0},
        {width, height},
        {0,     height}
    }, transform);
}

//
// Draw a filled or outlined ellipse
//
void drawEllipse(DrawMode mode, float x, float y, float radiusX, float radiusY, Mtx transform) {
    Mtx model;
    guMtxIdentity(model);

    // Move to x/y location
    Mtx temp;
    guMtxIdentity(temp);
    guMtxTrans(temp, x, y, 0);
    guMtxConcat(model, temp, model);

    // Apply additional transforms (if any)
    if(transform) {
        guMtxConcat(model, transform, model);
    }

    // Load position data
    GX_LoadPosMtxImm(model, GX_PNMTX0);

    // How many line segments make up the ellipse
    u8 segments = 100;
    f32 angleIncrement = M_PI*2.0f / segments;

    if(mode == DRAW_MODE_FILL) {
        GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, segments + 2);

        // Start at the center
        GX_Position2f32(0.0f, 0.0f);
        GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);

        // Draw spokes around the radius
        for (f32 theta = 0.0f; theta < 2.0f*M_PI + angleIncrement; theta += angleIncrement) {
            f32 x = radiusX * cos (theta);
            f32 y = radiusY * sin (theta);

            GX_Position2f32(x, y);
            GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);
        }

        GX_End();
    } else {
        GX_Begin(GX_LINESTRIP, GX_VTXFMT0, segments + 1);
        for (f32 theta = 0.0f; theta < 2.0f*M_PI + angleIncrement; theta += angleIncrement) {
            f32 x = radiusX * cos (theta);
            f32 y = radiusY * sin (theta);

            GX_Position2f32(x, y);
            GX_Color4u8(drawColor.r, drawColor.g, drawColor.b, drawColor.a);
        }
        GX_End();
    }
}