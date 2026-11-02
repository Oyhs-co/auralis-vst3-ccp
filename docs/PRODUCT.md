# PRODUCT.md — Definición de producto

> Fuente de verdad: `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md`. Este documento es un resumen operativo.

## Qué es Auralis

Plugin VST3 de mejora de audio, libre y de código abierto (AGPL-3.0-only), desarrollado en C++20 y JUCE. Ofrece un motor DSP modular que procesa la señal en cadena: ecualización, realce de graves, claridad, dinámica, ambiente y estéreo, con salida limitada. Diseñado para ser seguro en tiempo real, reproducible en compilación y desarrollado de forma transparente.

## Usuario objetivo

- Usuario de FL Studio (host prioritario) y otros DAWs en Windows 10/11 x64.
- Perfiles: productor/músico que quiere mejorar mezclas o monitores sin configuración compleja; oyente/entusiasta que busca una mejora de audio clara y controlable.
- No es el objetivo el usuario profesional que requiere suites de mastering de pago ni flujos con perfiles de calibración de sala.

## Propuesta de valor

- Mejora de audio integral y **gratuita** en un solo plugin, sin suscripciones ni telemetría.
- Motor DSP **propio e independiente del host**: la lógica no queda acoplada a JUCE ni al DAW.
- Cadena DSP fija y modular: cada módulo se valida por separado y se compone en orden fijo.
- Código abierto verificable: cualquier persona puede compilar, auditar y contribuir; licencia AGPL-3.0-only.

## Fuera de alcance

- Formatos distintos de VST3 y plataformas distintas de Windows x64 (por ahora).
- Grafo DSP dinámico: la cadena es fija; el reordenado dinámico no está contemplado.
- Soporte de hosts específicos no validados, presets de terceros, y funciones no definidas en el plan.
- Edición cerrada o binarios sin código fuente correspondiente.

## MVP

El MVP es la suma de los slices 0–9: proyecto compilable y reproducible, plugin VST3 cargable en FL Studio, y la cadena DSP completa (EQ → Bass → Clarity → Dynamics → Ambience → Stereo → Limiter) con parámetros seguros en tiempo real, bypass y estado persistente. Los slices 10–12 (analizador, presets/A-B/GUI, integración y distribución) completan el producto, no el MVP.

El progreso se mide por slices (0–12) con criterios de aceptación definidos en el plan (§7); nunca se implementa por delante del slice actual.
