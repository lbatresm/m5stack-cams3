#pragma once

#include <cstddef>
#include <cstdint>

bool board_sd_mount();
void board_sd_unmount();
bool board_sd_ok();
bool board_sd_mkdir(const char* path);
bool board_sd_write_file(const char* path, const uint8_t* data, size_t len);
