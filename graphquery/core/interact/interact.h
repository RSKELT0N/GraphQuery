#pragma once

namespace graphquery::interact
{
    class IInteract
    {
    public:
        explicit IInteract() = default;
        virtual ~IInteract() = default;

    public:
        [[nodiscard]] virtual int Initialise() noexcept = 0;
        virtual void Render() noexcept = 0;
    };
}