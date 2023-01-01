#pragma once

#include <cstddef>

class BitForward {
    public:
        virtual bool operator[](size_t index) = 0;
        virtual void operator[](size_t index, bool value) = 0;
}