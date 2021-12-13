#pragma once
#include <array>
#include <vector>
#include "definitioner.h"

//// Public Declarations [Interface] //////////////////////////////////////////////////////////////////////////////////////////////

namespace decoder
{
    // Public Members
    enum class state {
        uninitialized,
        running,
        working
    };

    // Public Methods ÆNDRET
    void run(bool allowPlayback = false);
    void end();
    //void appendQueue(std::vector<short> samples);
    void threadInstant();
    int									extractFrequency(std::vector<int>);
}