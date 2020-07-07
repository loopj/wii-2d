#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>

#include "window.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

// Screen information
GXRModeObj *rmode = NULL;

// Video memory
static void *frameBuffer[2] = { NULL, NULL};
u32	fb = 0;
void *gp_fifo = NULL;

// Are we rendering the first frame
bool first_frame = true;

//
// Initialize VI and GX
//
void initVideo() {
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
void shutdownVideo() {
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
void flushVideo() {
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
int getWidth() {
    return rmode->fbWidth;
}

//
// Get the screen height
//
int getHeight() {
    return rmode->efbHeight;
}