#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace vulkaninja
{
    template<typename T>
    class ArrayProxy
    {
    public:
        constexpr ArrayProxy() noexcept : m_Count(0), m_SelfPtr(nullptr) {}

        constexpr explicit ArrayProxy(std::nullptr_t) noexcept : m_Count(0), m_SelfPtr(nullptr) {}

        explicit ArrayProxy(T const& value) noexcept : m_Count(1), m_SelfPtr(&value) {}

        ArrayProxy(uint32_t count, T const* ptr) noexcept : m_Count(count), m_SelfPtr(ptr) {}

        template<std::size_t C>
        explicit ArrayProxy(T const (&ptr)[C]) noexcept : m_Count(C), m_SelfPtr(ptr)
        {}

#if __GNUC__ >= 9
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winit-list-lifetime"
#endif

        ArrayProxy(std::initializer_list<T> const& list) noexcept :
            m_Count(static_cast<uint32_t>(list.size())), m_SelfPtr(list.begin())
        {}

        template<typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
        ArrayProxy(std::initializer_list<typename std::remove_const<T>::type> const& list) noexcept :
            m_Count(static_cast<uint32_t>(list.size())), m_SelfPtr(list.begin())
        {}

#if __GNUC__ >= 9
#pragma GCC diagnostic pop
#endif

        template<typename V,
                 typename std::enable_if<
                     std::is_convertible<decltype(std::declval<V>().data()), T*>::value &&
                     std::is_convertible<decltype(std::declval<V>().size()), std::size_t>::value>::type* = nullptr>
        explicit ArrayProxy(V const& v) noexcept : m_Count(static_cast<uint32_t>(v.size())), m_SelfPtr(v.data())
        {}

        template<typename V,
                 typename std::enable_if<std::is_convertible<decltype(std::declval<V>().data()), T*>::value>::type* =
                     nullptr>
        ArrayProxy(V const& v, uint32_t startIndex, uint32_t count) noexcept :
            m_Count(count), m_SelfPtr(v.data() + startIndex)
        {}

        const T* begin() const noexcept { return m_SelfPtr; }

        const T* end() const noexcept { return m_SelfPtr + m_Count; }

        const T& front() const noexcept
        {
            assert(m_Count > 0 && m_SelfPtr);
            return *m_SelfPtr;
        }

        const T& back() const noexcept
        {
            assert(m_Count > 0 && m_SelfPtr);
            return *(m_SelfPtr + m_Count - 1);
        }

        bool empty() const noexcept { return (m_Count == 0); }

        uint32_t size() const noexcept { return m_Count; }

        T const* data() const noexcept { return m_SelfPtr; }

        bool contains(const T& x) const noexcept
        {
            for (uint32_t i = 0; i < m_Count; i++)
            {
                if (x == m_SelfPtr[i])
                {
                    return true;
                }
            }
            return false;
        }

        const T& operator[](uint32_t i) const noexcept
        {
            assert(0 <= i && i < m_Count);
            return m_SelfPtr[i];
        }

    private:
        uint32_t m_Count;
        const T* m_SelfPtr;
    };
} // namespace vulkaninja
