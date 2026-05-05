#pragma once

namespace OffsetsTable
{
    struct OffsetMapping
    {
        int       ID;
        uintptr_t STEAM_OFFSET;
        uintptr_t XBOX_OFFSET;
    };

    inline OffsetMapping offsetMappings[] = {
        {  147392, 0x2be72b0, 0x3858468 }, // sig
        {  147416, 0x2bea1f0, 0x385728C }, // sig
        {  470133, 0x4d5fc40, 0x2F6EA94 }, // vtable
        {  303817, 0x3bd3ee8, 0x0C6FE10 }, // vtable
        {  143812, 0x2a56a50, 0x371045C }, // sig
        {  145355, 0x2A81CE0, 0x376EA3C }, // better to reconstruct
        {  144161, 0x2a818d0, 0x372CE5C }, // sig
        {  883900, 0x5e7e2dc, 0x6A8C2B0 }, // instr
        {  142800, 0x29fffe0, 0x36846FC }, // sig
        {  497712, 0x4ee9470, 0x382321C }, // vtable
        {  937788, 0x61e51f0, 0x6A6A150 }, // instr
        {  944432, 0x60D7508, 0x6A8E530 }, // instr
        {  937583, 0x5fe18f8, 0x6866740 }, // instr
        {  944397, 0x60D73D8, 0x6A8C220 }, // instr
        {  936470, 0x5fb8668, 0x6AADB30 }, // instr
        {  922868, 0x5F4AFd0, 0x6701238 }, // instr
        {  141825, 0x298dd10, 0x35F06A0 }, // sig
        {  459617, 0x4d08350, 0x556CA28 }, // vtable
        {  472039, 0x4d6e928, 0x55D4A88 } // vtable
    };

    inline std::unordered_map<int, OffsetMapping> offsetMap = [] {
        std::unordered_map<int, OffsetMapping> map;
        for (const auto& mapping : offsetMappings) {
            map[mapping.ID] = mapping;
        }
        return map;
    }();

    inline uintptr_t GetOffset(int ID)
    {
        auto offset_map = offsetMap[ID];
        auto offset     = offset_map.STEAM_OFFSET;

#ifdef XBOX_STORE
        offset = offset_map.XBOX_OFFSET;
#endif

#ifndef USE_STARFIELD_SDK_LITE
        uintptr_t library_offset = REL::ID{ a_id }.offset();
        if (offset != library_offset) {
            spdlog::error("Offset does not match offset by ID for id={} scan={:x} lib={:x}", a_id, offset, library_offset);
        }
    }
#endif
    return offset;
}
} // namespace MemoryOffsets
