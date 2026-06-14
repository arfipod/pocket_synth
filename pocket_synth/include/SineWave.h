#pragma once

#include "IWave.h"

class SineWave : public IWave
{
    public:
        SineWave(float frequency = 440.0f, float phase = 0.0f);

        float getFrequency();
        void setFrequency(float);

        float getPhase();
        void setPhase(float);

        float nextSample();
    
    private:
        float frequency_;
        float phase_; // Phase is normalized between 0.0 and 1.0
};