#pragma once

#include "interact/interact.h"

namespace graphquery::interact
{
    class CInteractCMD final : public IInteract
    {
      public:
        explicit CInteractCMD();
        ~CInteractCMD() override = default;

        void clean_up() noexcept override;

      private:
        void render() noexcept override;
    };
} // namespace graphquery::interact
