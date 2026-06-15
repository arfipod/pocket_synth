#include "SineWave.h"
#include "main_params.h"
#include <math.h>

SineWave::SineWave(float frequency, float phase) : frequency_(frequency), phase_(phase) {}

float SineWave::getFrequency()
{
    return frequency_;
}

void SineWave::setFrequency(float frequency)
{
    frequency_ = frequency;
}

float SineWave::getPhase()
{
    return phase_;
}

void SineWave::setPhase(float phase)
{
    phase_ = phase;
}

float SineWave::nextSample()
{
    phase_ += frequency_ / SAMPLE_RATE;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
    }
    return sinf(2.0f * M_PI * phase_);
}