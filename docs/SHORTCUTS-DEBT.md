# SHORTCUTS-DEBT.md — Registro de deuda técnica

> Estado: **revisión 2026-11-07 — deuda S-01…S-05 y U-01…U-16 resuelta** (ver §2). Los shortcuts vigentes son los de nueva generación listados en §1, todos acotados y documentados en código con `// shortcut:`.
> Fuente de verdad: `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md`; contrato de shortcuts: `AGENTS.md` §4.
> Documentos relacionados: [ARCHITECTURE.md](ARCHITECTURE.md) · [DSP-ALGORITHMS.md](DSP-ALGORITHMS.md) · [INTERFACES.md](INTERFACES.md) · [PARAMETERS.md](PARAMETERS.md) · [RUNTIME-FLOWS.md](RUNTIME-FLOWS.md).

Inventario reproducible:

```powershell
Select-String -Path Source,modules,tests,tools -Pattern '// shortcut:' -Recurse
```

---

## 1. Shortcuts vigentes (4)

| ID | Ubicación | Recorte | Cuándo duele | Upgrade |
|---|---|---|---|---|
| S-06 | `modules/dsp/clarity/ClarityProcessor.cpp:32` y `modules/dsp/limiter/LimiterProcessor.cpp:40` | Canales por encima del `numChannels` preparado pasan secos | Si el host entregara más canales que los preparados en `prepareToPlay` | Re-preparar al cambio de layout (los buses hoy son solo mono/estéreo) |
| S-07 | `modules/dsp/ambience/AmbienceProcessor.cpp:85` | Posición de predelay cuantizada a muestras enteras y aplicada por bloque | Automatizando `amb_predelay` | Interpolación de retardo fraccional |
| S-08 | `modules/dsp/ambience/AmbienceProcessor.cpp:63` | Ganancias de decay recalculadas por bloque (8 `pow`) — los escalones quedan enmascarados por la cola | Automatización muy rápida de `amb_decay` | Suavizar ganancias por muestra o crossfade |
| — | `modules/dsp/limiter/LimiterProcessor.cpp:70` | El ceiling es un clamp duro (por diseño, documentado en el header) | Solo con señal sobre el techo | Es la garantía de seguridad; no se cambia |

Los tres primeros tienen su `// shortcut:` en el código con condición de upgrade observable, conforme a `AGENTS.md` §4.

---

## 2. Deuda resuelta (2026-11-07)

Historial de lo que este registro documentaba y cómo se resolvió. Referencia: commits de la semana del 2026-11-03 al 2026-11-07.

| ID | Deuda original | Resolución |
|---|---|---|
| S-01 | Sub protection de 12 dB/oct (1 biquad) | **Resuelto**: dos biquads RBJ en cascada (24 dB/oct, −6 dB en el corte). Test `TestBass::subProtectionAttenuatesSubs` actualizado (< 0.1 a una octava bajo el corte) |
| S-02 | Clarity solo excitaba 2 canales | **Resuelto**: estado por canal dimensionado en `prepare` vía `ProcessContext::numChannels`; test `TestClarity::fourChannelsAllExcited`. Queda S-06 para canales no preparados |
| S-03 | Coeficientes de EQ sin suavizado (zipper) | **Resuelto**: objetivos recalculados solo al cambiar valores e interpolación por muestra de los 5 coeficientes (5 ms). Tests: `smoothAutomationHasNoZipper` + los de forma reescalonados |
| S-04 | Limiter máximo 2 canales | **Resuelto**: delay lines por canal según `ProcessContext::numChannels`; mismos tests de techo aplican a todos los canales |
| S-05 | Pico de ventana O(N·L) en el limiter | **Resuelto**: máximo deslizante exacto (cola monótona, O(1) amortizado). Invariantes verificados por `TestLimiter::slidingMaxMatchesBruteForce` (passthrough alineado muestra a muestra, techo, GR) |
| U-01 | Bypass solo apagaba el master gain | **Resuelto**: bypass real — `pushNeutralParameters()` alimenta la cadena con valores neutros (passthrough exacto verificado), sin clics (suavizados internos) y con la latencia preservada (el ring del limiter sigue avanzando). Excepción documentada: picos > 0 dBFS tocan el clamp del limiter |
| U-02 | EQ recalculaba coeficientes cada bloque | **Resuelto** junto con S-03: `setBand` compara valores antes de marcar dirty; `updateBand` corre en el prólogo del bloque |
| U-03 | Activación individual sin exponer | **Resuelto**: 10 parámetros bool nuevos (IDs estables, solo añadidos): `eq_en`, `eq_low_en`, `eq_band1_en`, `eq_band2_en`, `eq_high_en`, `bass_en`, `clarity_en`, `comp_en`, `amb_en`, `stereo_en`. El limiter no es conmutable (latencia). La GUI los dibuja automáticamente |
| U-04 | Gain reduction no visible | **Resuelto**: atómicos de GR del compresor y limiter expuestos (`getGainReductionAtomic`) y mostrados en `SpectrumView` ("GR c / GR l") |
| U-05 | Analizador: FFT fija sin historial ni promediado | **Resuelto parcial**: ring buffer de 1024 (siempre analiza las últimas muestras, cualquier tamaño de bloque) + promediado exponencial por banda (0.7/0.3). Overlap de ventana queda fuera (monitorización visual, no medición) |
| U-06 | Parámetros audibles sin suavizar | **Resuelto**: `stereo_balance` (20 ms), `comp_ratio` y `comp_knee` (20 ms, ratio clampeado ≥ 1), `amb_damping` (50 ms, coeficiente), `amb_width` (20 ms). Quedan S-07/S-08 para predelay/decay (cuantización inherente, documentada) |
| U-07 | Sin tests de presets/estado | **Resuelto**: `TestPresets.cpp` (round-trip JSON, rechazo de schema desconocido, presets de fábrica bien formos con allow-list de 49 IDs) + la cadena completa cubierta por `TestResilience` y pluginval (estado, restauración, editor) |
| U-08 | Warning flags solo en el plugin | **Resuelto**: `juce_recommended_warning_flags` enlazado a `auralis_dsp`, `auralis_analysis`, `auralis_dsp_pipeline` y (por propagación) `auralis_tests`; los avisos destapados se corrigieron |
| U-09 | Estructura incompleta para release | **Resuelto lo exigible**: `docs/LICENSE` (CC BY-SA 4.0), plantillas de issue y PR, set documental completo (DSP-ALGORITHMS, PARAMETERS, INTERFACES, RUNTIME-FLOWS, BUILD-TEST-RELEASE, TROUBLESHOOTING, USER-GUIDE). `domain/parameters/state` siguen integrados en el adaptador a propósito (un solo target; decisión documentada en ARCHITECTURE). Tag `v0.2.0` al publicar |
| U-10 | Presets dependían de JUCE sin regla clara | **Resuelto por decisión (a)**: `AGENTS.md` §2 enmendado — `Source/presets` puede usar `juce_core` (JSON), nunca `juce_audio_processors`/`juce_gui_basics` |
| U-11 | Migración stub | **Sin cambio (correcto)**: solo existe schema v1; el rechazo de versiones desconocidas y el hook en `setStateInformation` son el plan de migración. La primera migración real entra en el PR que suba `schemaVersion`, con test |
| U-12 | pluginval sin verificar hash en CI | **Resuelto**: el workflow compara el SHA-256 del zip contra el hash fijado antes de ejecutarlo |
| U-13 | Compresor con `log10`+`pow` por muestra | **Resuelto**: reescrito a `log2`/`exp2` con constantes precomputadas (`kDbPerLog2`), matemática idéntica; tests de curva verifican equivalencia |
| U-14 | API sin uso (`getCorrelation`, `DspPipeline::reset`) | **Resuelto**: `getCorrelation()` eliminado (la GUI usa el atómico); `pipeline.reset()` se llama en `releaseResources` |
| U-15 | Docs desactualizadas | **Resuelto**: README con conteo correcto (49 IDs), `DSP-ALGORITHMS.md` creado, enlaces desde ARCHITECTURE, este registro reescrito |
| U-16 | Tests de NaN/inf solo en Gain | **Resuelto**: `TestResilience.cpp` — cadena completa (8 procesadores) con NaN/±inf a 44.1/48/88.2/96 kHz, bloques de 1 muestra y alternancia limpio/basura. El guard se aplica en `DspPipeline::process` (única entrada real), que sanea la entrada para que ningún estado se envenene |

---

## 3. Protocolo de shortcuts

### 3.1 Cómo añadir uno

1. **Código**: comentario en la línea exacta del límite, formato de `AGENTS.md` §4:
   ```cpp
   // shortcut: <límite concreto y medible>, <cuándo actualizar>
   ```
   El "cuándo" debe ser un hecho observable ("if profiling shows cost", "when multichannel support lands"), nunca "later" o "if needed".
2. **Este documento**: nueva fila en §1 con el siguiente ID libre (S-09 en adelante).
3. **PR**: describir el recorte y por qué el arreglo no entra ahora.
4. Si el límite es **audible en uso normal** o rompe una aceptación de slice, no es un shortcut: es un bug y va a issue.
5. Si una feature futura activa el límite, la entrada pasa a condición de bloqueo del PR que introduzca esa feature.

### 3.2 Cuándo un shortcut bloquea un release

Bloquea el release si: incumple un criterio de aceptación entregado, hace que la salida no corresponda al contrato del host (bypass, latencia), compromete la seguridad de tiempo real (asignación, lock, E/S en la ruta de audio — nunca se negocia), o implica pérdida de estado/presets.

Ninguno de los 4 vigentes bloquea: S-06 solo es alcanzable con layouts multicanal (fuera de alcance declarado), S-07/S-08 son inaudibles salvo automatización extrema y están declarados como limitaciones conocidas.

---

## 4. Verificación de la resolución

- `cmake --preset msvc` + `cmake --build --preset release`: 0 errores, 0 warnings (warning flags en todos los targets internos).
- `ctest`: todas las suites (EQ, Bass, Clarity, Dynamics, Ambience, Stereo, Limiter, FFT, Analyzer, Resilience, Presets, pipeline) — ALL TESTS PASSED.
- `pluginval --strictness-level 8`: SUCCESS (incluye editor, estado, restauración, automatización con sub-bloques, fuzzing).
- Documentación alineada: `PARAMETERS.md` (49 IDs, DSP actual), `DSP-ALGORITHMS.md` (algoritmos vigentes), `INTERFACES.md` (contratos corregidos), README (features reales).

---

*Documentación original bajo CC BY-SA 4.0 (plan §18.4). Última revisión: 2026-11-07.*
