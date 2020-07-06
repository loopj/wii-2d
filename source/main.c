#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
 
#define DEFAULT_FIFO_SIZE	(256*1024)

// Some types
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

// Screen information
GXRModeObj *rmode = NULL;

// Video memory
static void *frameBuffer[2] = { NULL, NULL};
u32	fb = 0;
void *gp_fifo = NULL;

// Are we rendering the first frame
bool first_frame = true;

// Keep track of current draw color
Color draw_color;

//
// Initialize VI and GX
//
void init_video() {
    // Initialize the VI subsystem
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    if(rmode == NULL) {
        exit(0);
        return;
    }

    // Allocate 2 framebuffers for double buffering
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Check if they were allocated
    if(frameBuffer[0] == NULL || frameBuffer[1] == NULL){
        exit(0);
        return;
    }

    // Configure VI subsystem with our initial framebuffer
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    // Initialize the GX subsystem with a FIFO
    gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
    memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

    // Set the initial background color
    GXColor black = {0, 0, 0, 0xff};
    GX_SetCopyClear(black, GX_MAX_Z24);

    // Set up the display
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    
    // Some additional Init code
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2*rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

    if(rmode->aa){
        GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    }else{
        GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    }

    GX_SetCullMode(GX_CULL_NONE);
    GX_CopyDisp(frameBuffer[fb],GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);


    // Set current vertex descriptor to enable position and color0
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Position has 2 elements (x,y), each of type f32
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);

    // Color 0 has 4 components (r, g, b, a), each component is 8b.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    // Color = vertex color
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);

    // Don't use textures
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);	
    GX_InvalidateTexAll();    

    // Set up the model view matrix
    Mtx GXmodelView2D;
    guMtxIdentity(GXmodelView2D);
    guMtxTransApply(GXmodelView2D, GXmodelView2D, 0.0f, 0.0f, -5.0f);
    GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

    // Set up the (2D) projection matrix
    Mtx44 projection;
    guOrtho(projection, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
}

//
// Clean up video memory
//
void stop_video() {
    // Abort drawing
    GX_AbortFrame();
    GX_Flush();

    // Release framebuffer memory
    free(MEM_K1_TO_K0(frameBuffer[0]));
    free(MEM_K1_TO_K0(frameBuffer[1]));
    frameBuffer[0] = NULL;
    frameBuffer[1] = NULL;

    // Release fifo memory
    free(gp_fifo);
    gp_fifo = NULL;
}

//
// Flush video changes to the screen
//
void flush_video() {
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(frameBuffer[fb],GX_TRUE);
    GX_DrawDone();

    VIDEO_SetNextFramebuffer(frameBuffer[fb]);

    // Enable video after rendering our first frame
    if(first_frame) {
        VIDEO_SetBlack(false);
        first_frame = false;
    }

    VIDEO_Flush();
    VIDEO_WaitVSync();
    fb ^= 1;
}

//
// Get the screen width
//
int get_width() {
    return rmode->fbWidth;
}

//
// Get the screen height
//
int get_height() {
    return rmode->efbHeight;
}

//
// Draw a filled or outlined polygon
//
void draw_polygon(DrawMode mode, u32 count, Vertex *vertices, Mtx transform) {
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
        GX_Begin(GX_QUADS, GX_VTXFMT0, count);
        for(int i = 0; i < count; i++) {
            GX_Position2f32(vertices[i].x, vertices[i].y);
            GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
        }
        GX_End();
    } else {
        GX_Begin(GX_LINESTRIP, GX_VTXFMT0, count + 1);
        for(int i = 0; i < count + 1; i++) {
            GX_Position2f32(vertices[i % count].x, vertices[i % count].y);
            GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
        }
        GX_End();
    }
}

//
// Draw a line or a set of lines
//
void draw_line(u32 count, Vertex *vertices, Mtx transform) {
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
        GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
    }
    GX_End();
}

//
// Draw a filled or outlined rectangle
//
void draw_rectangle(DrawMode mode, f32 x, f32 y, f32 width, f32 height, Mtx transform) {
    // Move to x/y location
    Mtx temp;
    guMtxIdentity(temp);
    guMtxTrans(temp, x, y, 0);
    if(transform) {
        guMtxConcat(transform, transform, temp);
    } else {
        transform = temp;
    }

    draw_polygon(mode, 4, (Vertex[4]){
        {0,     0},
        {width, 0},
        {width, height},
        {0,     height}
    }, transform);
}

//
// Draw a filled or outlined ellipse
//
void draw_ellipse(DrawMode mode, f32 x, f32 y, f32 radiusX, f32 radiusY, Mtx transform) {
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
        GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);

        // Draw spokes around the radius
        for (f32 theta = 0.0f; theta < 2.0f*M_PI + angleIncrement; theta += angleIncrement) {
            f32 x = radiusX * cos (theta);
            f32 y = radiusY * sin (theta);

            GX_Position2f32(x, y);
            GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
        }

        GX_End();
    } else {
        GX_Begin(GX_LINESTRIP, GX_VTXFMT0, segments + 1);
        for (f32 theta = 0.0f; theta < 2.0f*M_PI + angleIncrement; theta += angleIncrement) {
            f32 x = radiusX * cos (theta);
            f32 y = radiusY * sin (theta);

            GX_Position2f32(x, y);
            GX_Color4u8(draw_color.r, draw_color.g, draw_color.b, draw_color.a);
        }
        GX_End();
    }
}

//
// Set the current draw color
//
void set_color(Color color) {
    draw_color = color;
}

//
// Set the width of a line
//
void set_line_width(u8 width) {
    GX_SetLineWidth(width, GX_TO_ONE);
}

int main(){
    // Get things set up
    init_video();

    // Shut down cleanly if exit() is called
    int i = atexit(stop_video);
    if (i != 0) {
        exit(0);
    }

    set_line_width(20);

    // Do some drawing
    while(1) {
        set_color((Color){0xff, 0x00, 0x00, 0xff});
        draw_rectangle(DRAW_MODE_FILL, 0, 0, 100, 100, NULL);

        set_color((Color){0xff, 0xff, 0x00, 0xff});
        draw_rectangle(DRAW_MODE_OUTLINE, 120, 0, 100, 100, NULL);

        set_color((Color){0x00, 0xff, 0x00, 0xff});
        draw_ellipse(DRAW_MODE_FILL, 290, 50, 50, 50, NULL);

        set_color((Color){0x00, 0x00, 0xff, 0xff});
        draw_ellipse(DRAW_MODE_OUTLINE, 410, 50, 50, 50, NULL);

        set_color((Color){0x00, 0xff, 0xff, 0xff});
        draw_polygon(DRAW_MODE_FILL, 3, (Vertex[3]){
            {0, 120},
            {100, 120},
            {100, 220}
        }, NULL);

        set_color((Color){0xff, 0x00, 0xff, 0xff});
        draw_polygon(DRAW_MODE_OUTLINE, 3, (Vertex[3]){
            {120, 120},
            {220, 120},
            {220, 220}
        }, NULL);

        draw_line(4, (Vertex[4]){
            {240, 120},
            {340, 120},
            {240, 220},
            {340, 220}
        }, NULL);

        flush_video();
    }

    // Shut down cleanly
    stop_video();

    return 0;
}
 