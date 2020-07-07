#include <stdlib.h>

#include "window.h"
#include "shapes.h"

int main(){
    // Get things set up
    initVideo();

    // Shut down cleanly if exit() is called
    int i = atexit(shutdownVideo);
    if (i != 0) {
        exit(0);
    }

    setLineWidth(20);

    // Render a frame
    while(1) {
        setColor((Color){0xff, 0x00, 0x00, 0xff});
        drawRectangle(DRAW_MODE_FILL, 0, 0, 100, 100, NULL);

        setColor((Color){0xff, 0xff, 0x00, 0xff});
        drawRectangle(DRAW_MODE_OUTLINE, 120, 0, 100, 100, NULL);

        setColor((Color){0x00, 0xff, 0x00, 0xff});
        drawEllipse(DRAW_MODE_FILL, 290, 50, 50, 50, NULL);

        setColor((Color){0x00, 0x00, 0xff, 0xff});
        drawEllipse(DRAW_MODE_OUTLINE, 410, 50, 50, 50, NULL);

        setColor((Color){0x00, 0xff, 0xff, 0xff});
        drawPolygon(DRAW_MODE_FILL, 3, (Vertex[3]){
            {0, 120},
            {100, 120},
            {100, 220}
        }, NULL);

        setColor((Color){0xff, 0x00, 0xff, 0xff});
        drawPolygon(DRAW_MODE_OUTLINE, 3, (Vertex[3]){
            {120, 120},
            {220, 120},
            {220, 220}
        }, NULL);

        drawLine(4, (Vertex[4]){
            {240, 120},
            {340, 120},
            {240, 220},
            {340, 220}
        }, NULL);

        flushVideo();
    }

    // Shut down cleanly
    shutdownVideo();

    return 0;
}
 