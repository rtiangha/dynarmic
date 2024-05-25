/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/ir/location_descriptor.h"

namespace Dynarmic::IR {
std::string ToString(const LocationDescriptor& descriptor) {
    static const char* hex_digits = "0123456789ABCDEF";
    uint64_t value = descriptor.Value();
    return std::string("{") + hex_digits[(value >> 60) & 0xF] +
           hex_digits[(value >> 56) & 0xF] + hex_digits[(value >> 52) & 0xF] +
           hex_digits[(value >> 48) & 0xF] + hex_digits[(value >> 44) & 0xF] +
           hex_digits[(value >> 40) & 0xF] + hex_digits[(value >> 36) & 0xF] +
           hex_digits[(value >> 32) & 0xF] + hex_digits[(value >> 28) & 0xF] +
           hex_digits[(value >> 24) & 0xF] + hex_digits[(value >> 20) & 0xF] +
           hex_digits[(value >> 16) & 0xF] + hex_digits[(value >> 12) & 0xF] +
           hex_digits[(value >> 8) & 0xF] + hex_digits[(value >> 4) & 0xF] +
           hex_digits[value & 0xF] + "}";
}

void LocationDescriptor::Serialize(std::vector<uint16_t>& fres) const {
    u64 work_value = value;
    fres.push_back(static_cast<uint16_t>(work_value));
    work_value >>= 16;
    fres.push_back(static_cast<uint16_t>(work_value));
    work_value >>= 16;
    fres.push_back(static_cast<uint16_t>(work_value));
    work_value >>= 16;
    fres.push_back(static_cast<uint16_t>(work_value));
}

LocationDescriptor LocationDescriptor::Deserialize(std::vector<uint16_t>::iterator& it) {
    u64 value = *(it++);
    value |= static_cast<u64>(*(it++)) << 16;
    value |= static_cast<u64>(*(it++)) << 32;
    value |= static_cast<u64>(*(it++)) << 48;
    return LocationDescriptor(value);
}

}  // namespace Dynarmic::IR
