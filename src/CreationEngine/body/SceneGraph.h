#pragma once

#include <cstdint>
#include <string_view>

namespace RE
{
    class NiAVObject;
}

namespace body
{
    // Children of a scene graph node; zero for leaf objects.
    std::uint16_t ReadChildren(const RE::NiAVObject* node, RE::NiAVObject** out, std::uint16_t capacity);

    // Depth-first search below `node` for a node with this exact name.
    RE::NiAVObject* FindDescendant(RE::NiAVObject* node, std::string_view name, int depth = 0);

    // Case-insensitive substring test.
    bool Contains(std::string_view name, std::string_view part);
}
