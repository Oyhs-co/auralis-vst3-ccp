# ARCHITECTURE.md — Arquitectura

> Fuente de verdad: `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md`. Reglas aplicables en revisión: `AGENTS.md`.

## Diagrama de capas

```
Host (DAW)
  ↓ flujo de audio + callbacks
JUCE Plugin Adapter (Source/plugin)   ← única capa con juce_*: VST3, prepareToPlay/processBlock, estado
  ↓ contrato propio (sin tipos JUCE)
Domain (Source/domain, parameters, state)
  ↓
DSP Pipeline (modules/dsp_pipeline)
  ├─→ Processors (modules/dsp/<efecto>)
  ├─→ Analysis (Source/analysis, modules/telemetry)
  └─→ GUI (Source/ui — lee parámetros, nunca toca buffers de audio)
```

## Límites de responsabilidad (resumen de §4 del plan)

| Capa | Puede conocer | Nunca |
|---|---|---|
| Plugin adapter | dominio, parámetros, estado, JUCE | Algoritmos DSP, lógica de presets |
| Domain | parámetros, estado, contrato `audio_core` | Cualquier cabecera `juce_*`, ficheros, UI |
| Parameters / State | (sí mismos / parámetros) | `juce_audio_processors`, `juce_gui_basics`, ficheros, audio |
| DSP processors | contrato `audio_core` | `juce_*`, Componentes, ficheros, registro, red |
| DSP pipeline | `modules/dsp`, `audio_core` | JUCE, lógica del host |
| Presets | parámetros, estado | Procesado de audio; E/S de ficheros solo en callbacks no-RT |
| Analysis | `audio_core` | Nada que bloquee o bloquee el hilo de audio |
| GUI | JUCE, parámetros | Manipular buffers de audio directamente |

Regla dura: JUCE vive solo en `Source/plugin` (y `Source/ui`); DSP y dominio compilan y se prueban sin host ni GUI.

## Contrato de procesador

Cada efecto en `modules/dsp/<efecto>/` implementa la misma interfaz de tres operaciones:

- `prepare(spec)` — configura sample rate, tamaño de bloque y estado interno; toda asignación y pre-cálculo ocurre aquí, nunca en `process`.
- `process(...)` — procesa bloques; solo computación, sin asignación.
- `reset()` — vuelve al estado inicial (limpia retardos, envolventes, etc.).

El pipeline compone los procesadores en una única posición de cableado; nunca se añade un efecto inline en `PluginProcessor` ni dentro de otro efecto.

## Reglas de tiempo real para `processBlock`

- Sin asignación ni liberación de heap (`new`, `malloc`, `std::vector::resize`, `std::string`, excepciones).
- Sin mutexes, locks ni esperas entre hilos; sin E/S de ficheros o registro; sin llamadas al message thread; sin logging.
- Sin bucles no acotados: los límites derivan del tamaño de bloque y la configuración conocida.
- Parámetros se leen **una vez por bloque** mediante punteros `std::atomic` cacheados (p.ej. `APVTS::getRawParameterValue` cacheado en `prepareToPlay`), nunca por nombre por bloque.
- Parámetros audibles (ganancias, wet/dry) se suavizan (`SmoothedValue` o equivalente); ningún salto de parámetro entra directo a la señal.
- Todo lo preparable se hace en `prepareToPlay` (asignación de buffers, filtros, configuración de sample rate).
- Sin propagación de NaN/inf: procesar en `float`, proteger las rutas de realimentación.
- Comunicación audio ↔ GUI/control: atómicos para valores simples, colas SPSC lock-free para mensajes, buffers pre-asignados para telemetría; política de colas llenas definida de antemano. Nada de `APVTS` es RT-safe por defecto.

## Estado actual

Slice 0 (proyecto CMake + JUCE compilable, CI) y Slice 1 (passthrough + master gain con bypass, automatización y estado) en progreso. Nada por delante del slice actual.

## Cadena DSP planificada (orden fijo)

`Input → EQ → Bass Enhancement → Clarity → Dynamics → Ambience → Stereo Enhancement → Limiter → Output`

Módulos incorporados por slices 3–9 en ese mismo orden; analizador y metering en el slice 10, presets/A-B/GUI en el 11, integración y distribución en el 12.
