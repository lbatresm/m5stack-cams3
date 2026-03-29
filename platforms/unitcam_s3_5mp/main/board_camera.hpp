#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

bool board_camera_init();
bool board_camera_grab_jpeg(std::vector<uint8_t>& out);
