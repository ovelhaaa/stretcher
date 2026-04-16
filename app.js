let audioContext;
let fdtsmNode;
let mediaStreamSource;
let mediaElementSource;

const btnMic = document.getElementById('btn-mic');
const btnPlay = document.getElementById('btn-play');
const audioFileInput = document.getElementById('audio-file');
const audioPlayer = document.getElementById('audio-player');

const lowStretchSlider = document.getElementById('low-stretch');
const highStretchSlider = document.getElementById('high-stretch');
const lowValDisplay = document.getElementById('low-val');
const highValDisplay = document.getElementById('high-val');

async function initAudio() {
    if (audioContext) return;

    audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 44100 });

    try {
        const workletUrl = new URL('fdtsm_worklet.js', window.location.href);
        await audioContext.audioWorklet.addModule(workletUrl);

        fdtsmNode = new AudioWorkletNode(audioContext, 'fdtsm-processor', {
            outputChannelCount: [1], // Saída Mono
        });

        fdtsmNode.connect(audioContext.destination);

        btnPlay.textContent = 'AudioContext Ativo';
        btnPlay.classList.add('active');
        btnPlay.disabled = true;

        // Inicializa os parâmetros no worklet
        updateParams();

    } catch (e) {
        console.error('Erro ao carregar AudioWorklet:', e);
        alert('Erro ao iniciar o processador de áudio. Verifica a consola. Lembra-te de usar um servidor local!');
    }
}

btnPlay.addEventListener('click', async () => {
    await initAudio();
    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }
});

btnMic.addEventListener('click', async () => {
    await initAudio();
    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }

    try {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: {
            echoCancellation: false,
            noiseSuppression: false,
            autoGainControl: false
        }});

        if (mediaStreamSource) mediaStreamSource.disconnect();
        if (mediaElementSource) mediaElementSource.disconnect();

        mediaStreamSource = audioContext.createMediaStreamSource(stream);
        mediaStreamSource.connect(fdtsmNode);

        btnMic.classList.add('active');
        audioPlayer.style.display = 'none';
        audioPlayer.pause();
    } catch (err) {
        console.error('Erro ao acessar o microfone:', err);
        alert('Não foi possível acessar o microfone.');
    }
});

audioFileInput.addEventListener('change', async (e) => {
    await initAudio();
    if (audioContext.state === 'suspended') {
        await audioContext.resume();
    }

    const file = e.target.files[0];
    if (!file) return;

    const fileUrl = URL.createObjectURL(file);
    audioPlayer.src = fileUrl;
    audioPlayer.style.display = 'block';

    // Desliga o microfone caso esteja ligado
    if (mediaStreamSource) {
        mediaStreamSource.disconnect();
        btnMic.classList.remove('active');
    }

    // CORREÇÃO 1: Evitar o erro de recriar o MediaElementSource
    // Só criamos o nó se ainda não tiver sido criado.
    if (!mediaElementSource) {
        mediaElementSource = audioContext.createMediaElementSource(audioPlayer);
    } else {
        mediaElementSource.disconnect(); // Apenas o desligamos para limpar a rota anterior
    }

    // Conectamos a música carregada ao nosso efeito (fdtsmNode)
    mediaElementSource.connect(fdtsmNode);

    // CORREÇÃO 2: Damos a instrução para a música começar a tocar
    audioPlayer.play().catch(erro => {
        console.log("Erro ao tentar tocar automaticamente:", erro);
    });
});

function updateParams() {
    if (!fdtsmNode) return;
    fdtsmNode.port.postMessage({
        type: 'params',
        lowStretch: parseFloat(lowStretchSlider.value) / 100.0,
        highStretch: parseFloat(highStretchSlider.value) / 100.0
    });
}

lowStretchSlider.addEventListener('input', (e) => {
    lowValDisplay.textContent = e.target.value;
    updateParams();
});

highStretchSlider.addEventListener('input', (e) => {
    highValDisplay.textContent = e.target.value;
    updateParams();
});