#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace codec
{

namespace H264
{

constexpr size_t getPayloadDescriptorSize() {
    return 1;
}

constexpr bool isKeyFrame(const uint8_t* payload, const size_t payloadDescriptorSize)
{
    const uint8_t type = payload[0] & 0x1f;
    return payloadDescriptorSize != 0 && type == 0x7;
}

} // namespace H264

} // namespace codec
