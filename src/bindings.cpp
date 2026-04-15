#include <emscripten/bind.h>
#include "PhaseVocoderFDTSM.h"

using namespace emscripten;

// Helper to push float array from JS to C++
void pushInputWasm(PhaseVocoderFDTSM& pv, intptr_t inputPtr, int numSamples) {
    const float* input = reinterpret_cast<const float*>(inputPtr);
    pv.pushInput(input, numSamples);
}

// Helper to get float array from C++ to JS
int getOutputWasm(PhaseVocoderFDTSM& pv, intptr_t outputPtr, int maxSamples) {
    float* output = reinterpret_cast<float*>(outputPtr);
    return pv.getOutput(output, maxSamples);
}

EMSCRIPTEN_BINDINGS(phase_vocoder_module) {
    class_<PhaseVocoderFDTSM>("PhaseVocoderFDTSM")
        .constructor<int>()
        .function("setLowStretch", &PhaseVocoderFDTSM::setLowStretch)
        .function("setHighStretch", &PhaseVocoderFDTSM::setHighStretch)
        .function("pushInputWasm", &pushInputWasm)
        .function("getOutputWasm", &getOutputWasm);
}
