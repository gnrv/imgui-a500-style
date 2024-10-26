#pragma once

extern "C" {
void filter_init(const char *logfile);
void filter_gl_context_lost();
void filter_gl_context_restored();
void filter_draw(int tex);
// Called when filter needs to be redrawn
void filter_register_callback(void (*callback)());
}
