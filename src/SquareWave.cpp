#include "SquareWave.h"
#include "main_params.h"
#include <math.h>

SquareWave::SquareWave(float frequency, float phase) : frequency_(frequency), phase_(phase) {}

float SquareWave::getFrequency()
{
    return frequency_;
}

void SquareWave::setFrequency(float frequency)
{
    frequency_ = frequency;
}

float SquareWave::getPhase()
{
    return phase_;
}

void SquareWave::setPhase(float phase)
{
    phase_ = phase;
}

float SquareWave::nextSample()
{
    float outputSample{0};
    phase_ += frequency_ / SAMPLE_RATE;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
    }

    outputSample = (phase_ >= 0.0f && phase_ < 0.5f) ? 1.0f : -1.0f;

    return outputSample;
}