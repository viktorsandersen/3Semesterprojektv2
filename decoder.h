#pragma once
#include <array>
#include <vector>
#include "definitioner.h"

// Public Declarations [Interface]

namespace decoder
{
    // Public Members
    enum class state {
        uninitialized,
        running,
        working
    };

    // Public Methods
    void        run(bool allowPlayback = false);
    void        end();
    void        threadInstant();
    void		extractFrequency(std::vector<int>);
}