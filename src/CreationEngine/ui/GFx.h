#pragma once
// Minimal Scaleform GFx bindings; layouts follow SFSE and CommonLibSF.

#include <cstdint>
#include <CreationEngine/memory/ScanHelper.h>

namespace GFx
{
    class ObjectInterface;

    class Value
    {
    public:
        enum Type : std::uint32_t
        {
            kUndefined = 0,
            kNull,
            kBoolean,
            kInt,
            kUInt,
            kNumber,
            kString,
            kStringW,
            kObject,
            kArray,
            kDisplayObject,
            kFunction,
        };

        static constexpr std::uint32_t kTypeMask   = 0x0F;
        static constexpr std::uint32_t kManagedBit = 0x40;

        Value() = default;
        explicit Value(bool v) : m_type(kBoolean) { m_value.b = v; }
        explicit Value(double v) : m_type(kNumber) { m_value.n = v; }
        explicit Value(std::uint32_t v) : m_type(kUInt) { m_value.u = v; }
        explicit Value(const char* v) : m_type(kString) { m_value.s = v; }
        ~Value() { Release(); }

        Value(const Value&)            = delete;
        Value& operator=(const Value&) = delete;

        [[nodiscard]] Type GetType() const { return static_cast<Type>(m_type & kTypeMask); }
        [[nodiscard]] bool IsUndefined() const { return GetType() == kUndefined; }
        [[nodiscard]] bool IsObjectLike() const { return GetType() >= kObject && GetType() <= kFunction; }

        [[nodiscard]] bool          GetBool() const { return m_value.b; }
        [[nodiscard]] double        GetNumber() const;
        [[nodiscard]] const char*   GetString() const { return GetType() == kString ? m_value.s : nullptr; }
        [[nodiscard]] void*         GetData() const { return m_value.data; }

        bool          GetMember(const char* name, Value* out) const;
        bool          SetMember(const char* name, const Value& value);
        bool          Invoke(const char* name, Value* result, const Value* args, std::uint32_t argCount);
        std::uint32_t GetArraySize() const;
        bool          GetElement(std::uint32_t index, Value* out) const;
        bool          PushBack(const Value& value);

        void Release();

    private:
        [[nodiscard]] bool IsDisplayObject() const { return GetType() == kDisplayObject; }

        void*            m_unk00{ nullptr };            // 00
        void*            m_unk08{ nullptr };            // 08
        ObjectInterface* m_objectInterface{ nullptr };  // 10
        std::uint32_t    m_type{ kUndefined };          // 18
        union
        {
            std::int32_t  i;
            std::uint32_t u;
            double        n;
            bool          b;
            const char*   s;
            void*         data;
        } m_value{};                                     // 20
        std::uint64_t m_dataAux{ 0 };                    // 28
    };
    static_assert(sizeof(Value) == 0x30);

    struct FunctionParams
    {
        Value*        retVal;
        void*         movie;
        Value*        thisValue;
        Value*        argsWithThis;
        Value*        args;
        std::uint32_t argCount;
        void*         userData;
    };
    static_assert(sizeof(FunctionParams) == 0x38);

    // The initial reference is never released, so the engine never frees handlers.
    class FunctionHandler
    {
    public:
        virtual ~FunctionHandler() = default;
        virtual void Call(const FunctionParams* params) = 0;

    private:
        volatile long m_refCount{ 1 };
    };
    static_assert(sizeof(FunctionHandler) == 0x10);

    // ASMovieRootBase, at MovieImpl+0x18.
    class MovieRoot
    {
    public:
        static MovieRoot* FromMovie(void* movieImpl);

        void CreateObject(Value* out);
        void CreateArray(Value* out);
        void CreateFunction(Value* out, FunctionHandler* handler);
        bool GetVariable(Value* out, const char* path);
    };
}
