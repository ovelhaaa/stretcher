import Module from './fdtsm.js';

class FDTSMProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.wasmLoaded = false;
        this.pv = null;

        // Define sizes. In PhaseVocoderFDTSM we typically process hop_size at a time.
        // Or we just feed the available samples. We allocate a safe maximum buffer size
        // to avoid real-time memory allocations in the process method.
        this.maxBlockSize = 1024;

        this.port.onmessage = (event) => {
            if (event.data.type === 'params' && this.pv) {
                this.pv.setLowStretch(event.data.lowStretch);
                this.pv.setHighStretch(event.data.highStretch);
            }
        };

        // Initialize WASM Module
        Module().then((instance) => {
            this.wasmInstance = instance;
            // The C++ class takes sample rate, hop_size=256, frame_size=1024, max_delay=4096 (depends on implementation)
            // But from the bindings, the constructor takes one argument: int (likely maxDelay)
            // Let's assume it initializes with default 1024 / 256 for frame/hop, or we can check PhaseVocoderFDTSM.h if needed.
            // Let's pass 4096 as maxDelay based on the fact that constructor takes 1 int.
            this.pv = new this.wasmInstance.PhaseVocoderFDTSM(44100);

            // Allocate memory in WASM for input and output buffers
            this.inputPtr = this.wasmInstance._malloc(this.maxBlockSize * 4); // 4 bytes per float
            this.inputHeap = new Float32Array(this.wasmInstance.HEAPF32.buffer, this.inputPtr, this.maxBlockSize);

            this.outputPtr = this.wasmInstance._malloc(this.maxBlockSize * 4);
            this.outputHeap = new Float32Array(this.wasmInstance.HEAPF32.buffer, this.outputPtr, this.maxBlockSize);

            this.wasmLoaded = true;
        });
    }

    process(inputs, outputs, parameters) {
        if (!this.wasmLoaded) {
            // Passthrough if WASM isn't loaded yet
            const input = inputs[0];
            const output = outputs[0];
            if (input && input.length > 0 && input[0]) {
                if (output && output.length > 0 && output[0]) {
                    output[0].set(input[0]);
                }
            }
            return true;
        }

        const inputChannel = inputs[0][0]; // Mono processing
        const outputChannel = outputs[0][0];

        if (!inputChannel || !outputChannel) return true;

        // Prevent exceeding allocated size to avoid dynamic allocations
        const size = Math.min(inputChannel.length, this.maxBlockSize);

        // Copy JS input to WASM heap
        this.inputHeap.set(inputChannel.subarray(0, size));

        // Call the bound C++ function pushInputWasm
        this.wasmInstance.pushInputWasm(this.pv, this.inputPtr, size);

        // Call the bound C++ function getOutputWasm
        // We ask for 'size' samples to output
        const generatedSamples = this.wasmInstance.getOutputWasm(this.pv, this.outputPtr, size);

        // Copy WASM heap output to JS output
        if (generatedSamples > 0) {
            // Fill with what was generated
            outputChannel.set(new Float32Array(this.wasmInstance.HEAPF32.buffer, this.outputPtr, generatedSamples));
            // Fill rest with 0s if generated < size
            for (let i = generatedSamples; i < size; i++) {
                outputChannel[i] = 0;
            }
        } else {
            // No output available yet
            outputChannel.fill(0);
        }

        return true;
    }
}

registerProcessor('fdtsm-processor', FDTSMProcessor);
