# FDTSM Phase Vocoder (WebAssembly)

Este repositório contém a implementação em C++ de um Phase Vocoder FDTSM, compilado para WebAssembly e utilizado no navegador com Web Audio API. O código permite que os usuários apliquem alongamento de tempo em sinais de áudio usando controles de "graves/corpo" e "agudos/ataque" independentes em tempo real.

## Estrutura do Repositório

- `src/`
  - `PhaseVocoderFDTSM.h`: Header da classe C++ para processamento do Phase Vocoder FDTSM.
  - `PhaseVocoderFDTSM.cpp`: Implementação dos algoritmos em C++ focada em não fazer realocações de memória dinâmicas em processamento.
  - `bindings.cpp`: Código usando `embind` para amarrar os métodos de C++ e expô-los ao JavaScript.
- `index.html`: Interface visual (painel de pedal) do usuário.
- `app.js`: Script do frontend para criar o `AudioContext`, manusear os inputs (Microfone / Arquivo) e controlar os parâmetros.
- `fdtsm_worklet.js`: O `AudioWorkletProcessor` para carregar o módulo `.wasm` no lado do trabalhador de áudio (Worklet).
- `.github/workflows/deploy.yml`: Arquivo de GitHub Actions para configurar compilação automática e implantação no GitHub Pages.

## Automação com GitHub Pages (CI/CD)

Ao realizar um push para a branch `main`, o GitHub Actions disparará automaticamente um fluxo de trabalho. Este fluxo configurará o SDK do Emscripten, compilará os arquivos fonte C++ na pasta `src` em arquivos `fdtsm.js` e `fdtsm.wasm` e hospedará eles, junto do `index.html`, `app.js` e `fdtsm_worklet.js` através do **GitHub Pages**.

**Antes do primeiro push:**
- Certifique-se de que os caminhos definidos nos arquivos respeitem o mapeamento de sua URL no GitHub Pages. Devido ao Worklet carregar no escopo raiz, o módulo URL foi mantido sendo relativo no `app.js` e as importações usam arquivos que ficarão na mesma pasta após compilação.
- Uma vez ativado via Settings -> Pages, você deve apontar a origem de compilação da página para GitHub Actions e o artefato ficará visível.

## Compilação Local

Caso queira fazer testes e mudanças sem precisar subir para o GitHub:
1. Instale o Emscripten (https://emscripten.org/docs/getting_started/downloads.html).
2. Vá para a raiz do repositório no seu terminal e rode a compilação:

```bash
emcc src/PhaseVocoderFDTSM.cpp src/bindings.cpp \
     -o fdtsm.js \
     -lembind -O3 \
     -s EXPORT_ES6=1 -s MODULARIZE=1 -s ENVIRONMENT=worker \
     -s ALLOW_MEMORY_GROWTH=1 -s MALLOC=emmalloc \
     -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
```

Isso gerará os arquivos `fdtsm.js` e `fdtsm.wasm`. Depois, você pode usar uma extensão como "Live Server" no VS Code ou rodar `python3 -m http.server` para abrir o `index.html`. Não esqueça de acessar usando `localhost` ou `127.0.0.1`.
