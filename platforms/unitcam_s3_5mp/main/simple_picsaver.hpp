#pragma once

/** Mount SD, create /simple_picsaver, return false if SD missing. */
bool simple_picsaver_init();

/** Capture one JPEG and save (call ~1 Hz). */
void simple_picsaver_tick();
