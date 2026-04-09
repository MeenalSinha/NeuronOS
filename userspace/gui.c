// NeuronOS Framebuffer GUI Window Manager
// userspace/gui.c

#include <stdint.h>

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

typedef struct {
    int x, y, width, height;
    uint32_t bg_color;
    char title[32];
    bool is_active;
} window_t;

static window_t windows[16];
static uint32_t* framebuffer = (uint32_t*)0xFD000000; // Simulated LFB address

void gui_init(void) {
    // Syscall to map framebuffer
    
    // Clear screen to black
    for(int i=0; i<SCREEN_WIDTH*SCREEN_HEIGHT; i++) {
        framebuffer[i] = 0x000000;
    }
}

int gui_create_window(int x, int y, int width, int height, const char* title, uint32_t bg_color) {
    for (int i=0; i<16; i++) {
        if (!windows[i].is_active) {
            windows[i].x = x;
            windows[i].y = y;
            windows[i].width = width;
            windows[i].height = height;
            windows[i].bg_color = bg_color;
            
            // Copy title
            int j = 0;
            while(title[j] && j < 31) { windows[i].title[j] = title[j]; j++; }
            windows[i].title[j] = '\0';
            
            windows[i].is_active = true;
            return i;
        }
    }
    return -1;
}

void gui_render(void) {
    for (int i=0; i<16; i++) {
        if (windows[i].is_active) {
            // Draw window background
            for (int wy = 0; wy < windows[i].height; wy++) {
                for (int wx = 0; wx < windows[i].width; wx++) {
                    int screen_x = windows[i].x + wx;
                    int screen_y = windows[i].y + wy;
                    if (screen_x >= 0 && screen_x < SCREEN_WIDTH && screen_y >= 0 && screen_y < SCREEN_HEIGHT) {
                        framebuffer[screen_y * SCREEN_WIDTH + screen_x] = windows[i].bg_color;
                    }
                }
            }
        }
    }
}
