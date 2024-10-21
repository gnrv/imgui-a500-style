#pragma once

extern "C" {
void filter_init();
void filter_draw(int tex, int width, int height);
// Called when filter needs to be redrawn
void filter_register_callback(void (*callback)());
}
