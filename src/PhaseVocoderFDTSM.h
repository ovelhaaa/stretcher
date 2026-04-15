#ifndef PHASE_VOCODER_FDTSM_H
#define PHASE_VOCODER_FDTSM_H

#include <vector>
#include <complex>
#include <cstdint>

class PhaseVocoderFDTSM {
public:
    PhaseVocoderFDTSM(int sampleRate);
    ~PhaseVocoderFDTSM() = default;

    PhaseVocoderFDTSM(const PhaseVocoderFDTSM&) = delete;
    PhaseVocoderFDTSM& operator=(const PhaseVocoderFDTSM&) = delete;

    void setLowStretch(float alpha1);
    void setHighStretch(float alpha2);

    void pushInput(const float* input, int numSamples);
    int getOutput(float* output, int maxSamples);

private:
    int sampleRate;
    static constexpr int N = 1024;
    static constexpr int N2 = N / 2;
    static constexpr int HOP_A = N / 4; 
    
    float alpha1;
    float alpha2;

    std::vector<float> inFifo;
    std::vector<float> outFifo;
    std::vector<float> outFifo2; // second region output to handle different stretches separately
    
    int inFifoWritePtr;
    int inFifoReadPtr;
    int inFifoFill;
    
    int64_t synthesisHopCounter1;
    int64_t synthesisHopCounter2;
    int outFifoReadPtr1;
    int outFifoReadPtr2;
    int outFifoFill1;
    int outFifoFill2;

    std::vector<std::complex<float>> fftBuffer;
    std::vector<int> bitReverse;
    std::vector<std::complex<float>> twiddles;

    std::vector<float> window;
    
    std::vector<std::complex<float>> Y1;
    std::vector<std::complex<float>> Y2;

    std::vector<float> lastPhase;
    std::vector<float> sumPhase;

    static constexpr int REGION1_START = 0;
    static constexpr int REGION1_END = 100;
    static constexpr int REGION2_START = 101;
    static constexpr int REGION2_END = N2;

    void initFFT();
    void computeFFT(std::vector<std::complex<float>>& data, bool inverse);
    void processFrame();
    float principalArgument(float phase);
};

#endif
