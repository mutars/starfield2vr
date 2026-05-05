#pragma once
#include <utility/Module.hpp>
#include <utility/Scan.hpp>
#include <utility/RTTI.hpp>

namespace MemoryScan {
    const auto mod      = utility::get_executable();
    const size_t mod_size = utility::get_module_size(mod).value_or(0);
    const auto mod_end  = (uintptr_t)mod + mod_size - 0x100;


    inline uintptr_t VTable(const char* hook_name, const char* table, uintptr_t static_offset)
    {
        uintptr_t val = 0;
#if defined _DEBUG || defined SIGNATURE_SCAN
        auto ref = utility::rtti::find_vtable(mod, table);
        if (!ref) {
            spdlog::error("VTable pattern not found for id={}", hook_name );
            return 0;
        }
        val = ref.value();
        if(static_offset > 0) {
            auto offset = val - (uintptr_t)mod;
            if(offset != static_offset) {
                spdlog::warn("VTable offset mismatch id={}: scanned={:x} static={:x}", hook_name, offset, static_offset);
            }
        }
#else
        val =  static_offset + (uintptr_t)mod;
#endif
        return val;
    };

    inline uintptr_t FuncRelocation(const char* pattern, uintptr_t static_offset = 0 , uintptr_t a_id = 0) {
        uintptr_t val = 0;
#if defined _DEBUG || defined SIGNATURE_SCAN
        auto ref = utility::scan(mod, pattern);
        if (!ref) {
            spdlog::error("FuncRelocation pattern not found for id={}", a_id);
            return 0;
        }
        val = ref.value();
        if(static_offset > 0) {
            auto offset = val - (uintptr_t)mod;
            if(offset != static_offset) {
                spdlog::warn("FuncRelocation offset mismatch id={}: scanned={:x} static={:x}", a_id, offset, static_offset);
            }
        }
#else
        if(static_offset > 0) {
            val =  static_offset + (uintptr_t)mod;
        } else {
            spdlog::error("no offset is provided id={} ", a_id);
        }
#endif
        return val;
    };
    inline uintptr_t InstructionRelocation(const char* pattern, UINT offset_begin, UINT instruction_size,  uintptr_t static_offset = 0 , uintptr_t a_id = 0) {
        uintptr_t val = 0;
#if defined _DEBUG || defined SIGNATURE_SCAN
        auto ref = utility::scan(mod, pattern);
        if (!ref) {
            spdlog::error("AsmCodeRelocation pattern not found for id={}", a_id);
            return 0;
        }
        auto addr = ref.value();
        val = addr + *(int32_t*)(addr + offset_begin) + instruction_size;
        if(static_offset > 0) {
            auto offset = val - (uintptr_t)mod;
            if(offset != static_offset) {
                spdlog::warn("InstructionRelocation offset mismatch id={}: scanned={:x} static={:x}", a_id, offset, static_offset);
            }
        }
#else
        if(static_offset > 0) {
            val = static_offset + (uintptr_t)mod;
        } else {
            spdlog::error("no offset is provided id={} ", a_id);
        }
#endif
        return val;
    };    
}