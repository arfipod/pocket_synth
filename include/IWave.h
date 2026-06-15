#pragma once

// By default, amplitude is normalized to [-1 1]

class IWave 
{
    public:
        virtual float nextSample() = 0;
};