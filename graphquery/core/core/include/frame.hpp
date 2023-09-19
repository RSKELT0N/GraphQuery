#pragma once

namespace graphquery::gui
{
    class IFrame
    {
    public:
        virtual ~IFrame() = default;
        explicit IFrame() = default;

        IFrame(const IFrame &) = delete;
        IFrame(IFrame &&) = delete;
        IFrame & operator=(const IFrame &) = delete;
        IFrame & operator=(IFrame &&) = delete;

    public:
        virtual void Render_Frame() noexcept = 0;
    };
}