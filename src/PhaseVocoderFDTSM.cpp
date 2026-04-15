#include "PhaseVocoderFDTSM.h"
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PhaseVocoderFDTSM::PhaseVocoderFDTSM(int sampleRate)
    : sampleRate(sampleRate),
      alpha1(1.0f), alpha2(1.0f),
      inFifoWritePtr(0), inFifoReadPtr(0), inFifoFill(0),
      synthesisHopCounter1(0), synthesisHopCounter2(0),
      outFifoReadPtr1(0), outFifoReadPtr2(0), outFifoFill1(0), outFifoFill2(0)
{
    inFifo.resize(N * 4, 0.0f);
    outFifo.resize(N * 10, 0.0f);
    outFifo2.resize(N * 10, 0.0f);

    fftBuffer.resize(N, {0.0f, 0.0f});
    twiddles.resize(N2, {0.0f, 0.0f});
    bitReverse.resize(N, 0);

    window.resize(N, 0.0f);
    Y1.resize(N, {0.0f, 0.0f});
    Y2.resize(N, {0.0f, 0.0f});

    lastPhase.resize(N2 + 1, 0.0f);
    sumPhase.resize(N2 + 1, 0.0f);

    for (int i = 0; i < N; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (N - 1)));
    }

    initFFT();
}

void PhaseVocoderFDTSM::initFFT() {
    int bits = 0;
    while ((1 << bits) < N) bits++;

    for (int i = 0; i < N; ++i) {
        int rev = 0;
        for (int j = 0; j < bits; ++j) {
            rev = (rev << 1) | ((i >> j) & 1);
        }
        bitReverse[i] = rev;
    }

    for (int i = 0; i < N2; ++i) {
        float angle = -2.0f * M_PI * i / N;
        twiddles[i] = {std::cos(angle), std::sin(angle)};
    }
}

void PhaseVocoderFDTSM::computeFFT(std::vector<std::complex<float>>& data, bool inverse) {
    for (int i = 0; i < N; ++i) {
        int rev = bitReverse[i];
        if (i < rev) {
            std::swap(data[i], data[rev]);
        }
    }

    for (int len = 2; len <= N; len <<= 1) {
        int halfLen = len >> 1;
        int step = N / len;
        for (int i = 0; i < N; i += len) {
            for (int j = 0; j < halfLen; ++j) {
                std::complex<float> t = data[i + j + halfLen];
                std::complex<float> tw = twiddles[j * step];
                if (inverse) tw = std::conj(tw);
                std::complex<float> u = t * tw;
                data[i + j + halfLen] = data[i + j] - u;
                data[i + j] = data[i + j] + u;
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < N; ++i) {
            data[i] /= static_cast<float>(N);
        }
    }
}

void PhaseVocoderFDTSM::setLowStretch(float alpha) {
    alpha1 = std::max(0.25f, std::min(alpha, 4.0f));
}

void PhaseVocoderFDTSM::setHighStretch(float alpha) {
    alpha2 = std::max(0.25f, std::min(alpha, 4.0f));
}

float PhaseVocoderFDTSM::principalArgument(float phase) {
    return phase - 2.0f * M_PI * std::round(phase / (2.0f * M_PI));
}

void PhaseVocoderFDTSM::pushInput(const float* input, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        inFifo[inFifoWritePtr] = input[i];
        inFifoWritePtr = (inFifoWritePtr + 1) % inFifo.size();
        inFifoFill++;

        if (inFifoFill >= N) {
            processFrame();
            inFifoReadPtr = (inFifoReadPtr + HOP_A) % inFifo.size();
            inFifoFill -= HOP_A;
        }
    }
}

int PhaseVocoderFDTSM::getOutput(float* output, int maxSamples) {
    int samplesWritten = 0;

    // We output a sample only if both streams have generated one, and we sum them
    while (samplesWritten < maxSamples && outFifoFill1 > 0 && outFifoFill2 > 0) {
        float s1 = outFifo[outFifoReadPtr1];
        float s2 = outFifo2[outFifoReadPtr2];

        output[samplesWritten++] = s1 + s2;

        outFifo[outFifoReadPtr1] = 0.0f; // Clear after reading for OLA
        outFifo2[outFifoReadPtr2] = 0.0f;

        outFifoReadPtr1 = (outFifoReadPtr1 + 1) % outFifo.size();
        outFifoReadPtr2 = (outFifoReadPtr2 + 1) % outFifo2.size();
        outFifoFill1--;
        outFifoFill2--;
    }
    return samplesWritten;
}

void PhaseVocoderFDTSM::processFrame() {
    int readPtr = inFifoReadPtr;
    for (int i = 0; i < N; ++i) {
        fftBuffer[i] = {inFifo[(readPtr + i) % inFifo.size()] * window[i], 0.0f};
    }

    computeFFT(fftBuffer, false);

    int hopS1 = static_cast<int>(std::round(HOP_A * alpha1));
    int hopS2 = static_cast<int>(std::round(HOP_A * alpha2));

    // Clear pre-allocated Y1 and Y2 buffers
    for (int i = 0; i < N; ++i) {
        Y1[i] = {0.0f, 0.0f};
        Y2[i] = {0.0f, 0.0f};
    }

    for (int k = 0; k <= N2; ++k) {
        float mag = std::abs(fftBuffer[k]);
        float phase = std::arg(fftBuffer[k]);

        float omega = 2.0f * M_PI * k / N;
        float deltaPhase = phase - lastPhase[k] - omega * HOP_A;
        deltaPhase = principalArgument(deltaPhase);

        float instFreq = omega + deltaPhase / HOP_A;

        float S_sr;
        if (k >= REGION1_START && k <= REGION1_END) {
            S_sr = hopS1;
            sumPhase[k] = sumPhase[k] + instFreq * S_sr;
            Y1[k] = std::polar(mag, sumPhase[k]);
            if (k > 0 && k < N2) Y1[N - k] = std::conj(Y1[k]);
        } else {
            S_sr = hopS2;
            sumPhase[k] = sumPhase[k] + instFreq * S_sr;
            Y2[k] = std::polar(mag, sumPhase[k]);
            if (k > 0 && k < N2) Y2[N - k] = std::conj(Y2[k]);
        }

        lastPhase[k] = phase;
    }

    computeFFT(Y1, true);
    computeFFT(Y2, true);

    // OLA Region 1
    int writePtr1 = (outFifoReadPtr1 + outFifoFill1) % outFifo.size();
    for (int i = 0; i < N; ++i) {
        outFifo[(writePtr1 + i) % outFifo.size()] += Y1[i].real() * window[i] / hopS1;
    }
    outFifoFill1 += hopS1;

    // OLA Region 2
    int writePtr2 = (outFifoReadPtr2 + outFifoFill2) % outFifo2.size();
    for (int i = 0; i < N; ++i) {
        outFifo2[(writePtr2 + i) % outFifo2.size()] += Y2[i].real() * window[i] / hopS2;
    }
    outFifoFill2 += hopS2;
}
