#pragma once

#include <string>
#include "ReadingState.h"

namespace reading_state_codec {
bool encode(const ReadingState& state, std::string& output);
bool decode(const std::string& input, ReadingState& state, std::string& error);
}
