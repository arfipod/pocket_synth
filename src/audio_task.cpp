#include "pocketsynth_tasks.h"

#include "app_state.h"
#include "audio_output.h"
#include "audio_render.h"
#include "synth_config.h"

#include "freertos/FreeRTOS.h"

namespace pocketsynth {
namespace {

int32_t audioBuffer[(AUDIO_BUFFER_FRAMES + 1) / 2];

}  // namespace

void audioTask(void*) {
  for (;;) {
    SynthAudioState localState;
    copyAudioState(&localState);
    renderAudioBuffer(&localState, audioBuffer, AUDIO_BUFFER_FRAMES);
    storeRenderedAudioState(localState);
    writeAudioFrames(audioBuffer, AUDIO_BUFFER_FRAMES);
  }
}

}  // namespace pocketsynth
