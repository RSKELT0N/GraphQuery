/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file ring_buffer.hpp
 * \brief Header including implementation of ring_buffer data
 *        structure which can be used by other translation units
 *        to efficient store data in a ring-based buffer.
 ************************************************************/

#pragma once

#include "atomic_intrinsics.h"

#include <array>
#include <cassert>

namespace graphquery::database::utils
{
    template<typename T, std::size_t BufferSize>
    class CRingBuffer final
    {
      public:
        typedef T Type;
        typedef T & RefType;
        typedef T * PtrType;
        typedef const T & ConstRefType;
        typedef const T * ConstPtrType;

        struct SData
        {
            std::size_t head;
            std::size_t current_capacity;
            std::array<Type, BufferSize> * data;
        };

        inline constexpr CRingBuffer();

        void clear() noexcept;
        void add(const Type & elem) noexcept;
        Type operator[](std::size_t i) const noexcept;
        [[nodiscard]] std::size_t get_head() const noexcept;
        [[nodiscard]] std::size_t get_current_capacity() const noexcept;
        [[nodiscard]] std::size_t get_buffer_size() const noexcept;

      private:
        std::size_t m_tail          = 0;
        std::size_t m_curr_capacity = 0;
        std::array<Type, BufferSize> m_data;
    };

    //~ public:
    template<typename Type, std::size_t BufferSize>
    inline constexpr CRingBuffer<Type, BufferSize>::CRingBuffer()
    {
        m_data = std::array<Type, BufferSize>();
    }

    template<typename Type, std::size_t BufferSize>
    void CRingBuffer<Type, BufferSize>::clear() noexcept
    {
        m_curr_capacity = 0;
    }

    template<typename Type, std::size_t BufferSize>
    void CRingBuffer<Type, BufferSize>::add(ConstRefType elem) noexcept
    {
        m_data[m_tail]  = elem;
        m_tail          = (m_tail + 1) % BufferSize;
        m_curr_capacity = (m_curr_capacity < BufferSize) ? m_curr_capacity + 1 : BufferSize;
    }

    template<typename Type, std::size_t BufferSize>
    Type CRingBuffer<Type, BufferSize>::operator[](std::size_t i) const noexcept
    {
        assert(BufferSize > i && "Accessed element is out of range");
        return m_data.operator[](i);
    }

    template<typename Type, std::size_t BufferSize>
    std::size_t CRingBuffer<Type, BufferSize>::get_current_capacity() const noexcept
    {
        return this->m_curr_capacity;
    }

    template<typename Type, std::size_t BufferSize>
    std::size_t CRingBuffer<Type, BufferSize>::get_head() const noexcept
    {
        return ((this->m_tail - 1) - this->m_curr_capacity) % BufferSize;
    }

    template<typename Type, std::size_t BufferSize>
    std::size_t CRingBuffer<Type, BufferSize>::get_buffer_size() const noexcept
    {
        return BufferSize;
    }
} // namespace graphquery::database::utils
