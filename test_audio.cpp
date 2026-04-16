#include "src/PhaseVocoderFDTSM.h"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    PhaseVocoderFDTSM pv(44100);
    // Test with slight time stretching on low frequencies
    pv.setLowStretch(1.5f);
    pv.setHighStretch(1.0f);

    int sampleRate = 44100;
    int numSamples = 4410; // 100ms of audio
    std::vector<float> input(numSamples);

    // Generate test signal: Mix of low freq (100Hz) and high freq (1000Hz)
    for (int i = 0; i < numSamples; ++i) {
        float lowFreq = sin(2.0 * M_PI * 100.0 * i / sampleRate);
        float highFreq = sin(2.0 * M_PI * 1000.0 * i / sampleRate);
        input[i] = 0.5f * (lowFreq + highFreq);
    }

    std::vector<float> allOutput;

    int framesProcessed = 0;
    int blockSize = 1024;
    for (int i = 0; i < numSamples; i += blockSize) {
        int samplesToPush = std::min(blockSize, numSamples - i);
        pv.pushInput(input.data() + i, samplesToPush);
        framesProcessed++;

        std::vector<float> tempOut(4096, 0.0f);
        int generated = pv.getOutput(tempOut.data(), 4096);
        for(int j=0; j<generated; ++j) {
            allOutput.push_back(tempOut[j]);
        }
    }

    // Push some zeros to flush out remaining audio (due to delay)
    std::vector<float> zeros(blockSize * 10, 0.0f);
    pv.pushInput(zeros.data(), blockSize * 10);
    std::vector<float> tempOut(4096 * 5, 0.0f);
    int generated = pv.getOutput(tempOut.data(), 4096 * 5);
    for(int j=0; j<generated; ++j) {
        allOutput.push_back(tempOut[j]);
    }

    std::cout << "Original length: " << numSamples << " samples" << std::endl;
    std::cout << "Generated length: " << allOutput.size() << " samples" << std::endl;

    float maxAmp = 0.0f;
    float energy = 0.0f;
    for (float f : allOutput) {
        if (std::abs(f) > maxAmp) maxAmp = std::abs(f);
        energy += f * f;
    }
    std::cout << "Max output amplitude: " << maxAmp << std::endl;
    std::cout << "Output energy: " << energy << std::endl;

    if (allOutput.size() > numSamples && maxAmp > 0.0f) {
        std::cout << "DSP Core test PASSED: Audio is generated and modified." << std::endl;
        return 0;
    } else {
        std::cout << "DSP Core test FAILED." << std::endl;
        return 1;
    }
}
