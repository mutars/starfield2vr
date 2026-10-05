#include "GFx.h"

namespace GFx
{
    namespace
    {
        using GetMember_t    = bool (*)(ObjectInterface*, void* data, const char* name, Value* out, bool isDisplayObj);
        using SetMember_t    = bool (*)(ObjectInterface*, void* data, const char* name, const Value& value, bool isDisplayObj);
        using Invoke_t       = bool (*)(ObjectInterface*, void* data, Value* result, const char* name, const Value* args, std::uint64_t argCount, bool isDisplayObj);
        using GetArraySize_t = std::uint32_t (*)(ObjectInterface*, void* data);
        using GetElement_t   = bool (*)(ObjectInterface*, void* data, std::uint32_t index, Value* out);
        using PushBack_t     = bool (*)(ObjectInterface*, void* data, const Value& value);
        using Release_t      = void (*)(ObjectInterface*, Value* value, void* data);

        // Signatures for 1.16.244; the RVAs are the fallback.
        template <class T>
        T Resolve(const char* pattern, std::uintptr_t rva, std::uintptr_t id)
        {
            return reinterpret_cast<T>(MemoryScan::FuncRelocation(pattern, rva, id));
        }

        GetMember_t GetMemberFn()
        {
            static auto fn = Resolve<GetMember_t>("48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 D1 48 81 EC A0 00", 0x33528A0, 9001);
            return fn;
        }
        SetMember_t SetMemberFn()
        {
            static auto fn = Resolve<SetMember_t>("48 89 5C 24 20 4C 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 50 FC FF", 0x3352C90, 9002);
            return fn;
        }
        Invoke_t InvokeFn()
        {
            static auto fn = Resolve<Invoke_t>("48 8B C4 4C 89 40 18 55 53 48 8D 68 A8 48", 0x33530B0, 9003);
            return fn;
        }
        GetArraySize_t GetArraySizeFn()
        {
            static auto fn = Resolve<GetArraySize_t>("8B 42 40 C3 CC CC CC CC CC CC CC CC", 0x3354390, 9004);
            return fn;
        }
        GetElement_t GetElementFn()
        {
            static auto fn = Resolve<GetElement_t>("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B 41 18", 0x33543C0, 9005);
            return fn;
        }
        PushBack_t PushBackFn()
        {
            static auto fn = Resolve<PushBack_t>("40 53 48 83 EC 40 48 8B 41 08 4D 8B", 0x3354690, 9006);
            return fn;
        }
        Release_t ReleaseFn()
        {
            static auto fn = Resolve<Release_t>("40 53 48 83 EC 20 8B 42 18 48 8B DA 25 8F", 0x33524A0, 9007);
            return fn;
        }

        template <class Fn, class... Args>
        auto VCall(void* self, std::size_t index, Args... args)
        {
            auto vtable = *reinterpret_cast<void***>(self);
            return reinterpret_cast<Fn>(vtable[index])(self, args...);
        }
    }

    double Value::GetNumber() const
    {
        switch (GetType()) {
        case kInt:
            return m_value.i;
        case kUInt:
            return m_value.u;
        case kNumber:
            return m_value.n;
        case kBoolean:
            return m_value.b ? 1.0 : 0.0;
        default:
            return 0.0;
        }
    }

    bool Value::GetMember(const char* name, Value* out) const
    {
        auto fn = GetMemberFn();
        return fn && IsObjectLike() && fn(m_objectInterface, m_value.data, name, out, IsDisplayObject());
    }

    bool Value::SetMember(const char* name, const Value& value)
    {
        auto fn = SetMemberFn();
        return fn && IsObjectLike() && fn(m_objectInterface, m_value.data, name, value, IsDisplayObject());
    }

    bool Value::Invoke(const char* name, Value* result, const Value* args, std::uint32_t argCount)
    {
        auto fn = InvokeFn();
        return fn && IsObjectLike() && fn(m_objectInterface, m_value.data, result, name, args, argCount, IsDisplayObject());
    }

    std::uint32_t Value::GetArraySize() const
    {
        auto fn = GetArraySizeFn();
        return (fn && GetType() == kArray) ? fn(m_objectInterface, m_value.data) : 0;
    }

    bool Value::GetElement(std::uint32_t index, Value* out) const
    {
        auto fn = GetElementFn();
        return fn && GetType() == kArray && fn(m_objectInterface, m_value.data, index, out);
    }

    bool Value::PushBack(const Value& value)
    {
        auto fn = PushBackFn();
        return fn && GetType() == kArray && fn(m_objectInterface, m_value.data, value);
    }

    void Value::Release()
    {
        if ((m_type & kManagedBit) && m_objectInterface) {
            if (auto fn = ReleaseFn()) {
                fn(m_objectInterface, this, m_value.data);
            }
        }
        m_objectInterface = nullptr;
        m_type            = kUndefined;
        m_value.data      = nullptr;
    }

    MovieRoot* MovieRoot::FromMovie(void* movieImpl)
    {
        return movieImpl ? *reinterpret_cast<MovieRoot**>(static_cast<std::uint8_t*>(movieImpl) + 0x18) : nullptr;
    }

    // ASMovieRootBase vtable slots.
    void MovieRoot::CreateObject(Value* out)
    {
        VCall<void (*)(void*, Value*, const char*, const Value*, std::uint32_t)>(this, 0x2E, out, nullptr, nullptr, 0u);
    }

    void MovieRoot::CreateArray(Value* out)
    {
        VCall<void (*)(void*, Value*)>(this, 0x2F, out);
    }

    void MovieRoot::CreateFunction(Value* out, FunctionHandler* handler)
    {
        VCall<void (*)(void*, Value*, FunctionHandler*, void*)>(this, 0x30, out, handler, nullptr);
    }

    bool MovieRoot::GetVariable(Value* out, const char* path)
    {
        return VCall<bool (*)(void*, Value*, const char*)>(this, 0x32, out, path);
    }
}
