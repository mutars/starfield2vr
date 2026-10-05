#include "SceneGraph.h"

#include <algorithm>
#include <cctype>

#include <RE/N/NiAVObject.h>

namespace body
{
    // Leaf objects have no child array; read it under SEH so a non-node never crashes the walk.
    std::uint16_t ReadChildren(const RE::NiAVObject* node, RE::NiAVObject** out, std::uint16_t capacity)
    {
        std::uint16_t count = 0;
        __try {
            auto       as_node = reinterpret_cast<const RE::NiNode*>(node);
            const auto size    = as_node->children.m_size;
            if (as_node->children.entries && size <= as_node->children.m_arrayBufLen && size <= capacity) {
                for (std::uint16_t i = 0; i < size; ++i) {
                    out[count++] = as_node->children.entries[i];
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            count = 0;
        }
        return count;
    }

    RE::NiAVObject* FindDescendant(RE::NiAVObject* node, std::string_view name, int depth)
    {
        if (!node || depth > 40) {
            return nullptr;
        }
        RE::NiAVObject* children[128]{};
        const auto      count = ReadChildren(node, children, 128);
        for (std::uint16_t i = 0; i < count; ++i) {
            if (children[i] && name == children[i]->name.c_str()) {
                return children[i];
            }
        }
        for (std::uint16_t i = 0; i < count; ++i) {
            if (auto found = FindDescendant(children[i], name, depth + 1)) {
                return found;
            }
        }
        return nullptr;
    }

    bool Contains(std::string_view name, std::string_view part)
    {
        const auto it = std::search(name.begin(), name.end(), part.begin(), part.end(),
            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
        return it != name.end();
    }
}
