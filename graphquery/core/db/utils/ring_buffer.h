#pragma once

#include <array>

namespace graphquery::database::utils
{
    template<typename ElemType, size_t BufferSize>
    class CRingBuffer final
    {
    public:
        struct SData
        {
            std::array<ElemType, BufferSize>::iterator start;
            std::array<ElemType, BufferSize>::iterator end;
        };

        CRingBuffer();

    public:
        void Clear() noexcept;
        void Add_Element(const ElemType & elem) noexcept;
        SData Get_Data() const noexcept;

    private:
        std::size_t m_tail = 0;
        std::unique_ptr<std::array<ElemType, BufferSize>> m_data;
    };


    //~ public:
    template<typename ElemType, size_t BufferSize>
    graphquery::database::utils::CRingBuffer<ElemType, BufferSize>::CRingBuffer()
    {
        m_data = std::make_unique<std::array<ElemType, BufferSize>>();
    }

    template<typename ElemType, size_t BufferSize>
    void graphquery::database::utils::CRingBuffer<ElemType, BufferSize>::Clear() noexcept
    {
        m_tail = 0;
    }

    template<typename ElemType, size_t BufferSize>
    void graphquery::database::utils::CRingBuffer<ElemType, BufferSize>::Add_Element(const ElemType & elem) noexcept
    {
        (*m_data)[m_tail] = elem;
        m_tail = (m_tail + 1) % BufferSize;
    }

    template<typename ElemType, size_t BufferSize>
    graphquery::database::utils::CRingBuffer<ElemType, BufferSize>::SData
    graphquery::database::utils::CRingBuffer<ElemType, BufferSize>::Get_Data() const noexcept
    {
        return {.start = m_data->begin(), .end = m_data->begin() + m_tail};
    }
}