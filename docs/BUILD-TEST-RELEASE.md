# BUILD-TEST-RELEASE.md — Compilación, tests y release

> Fuente de verdad: `Plan_Auralis_VST3_Cpp_JUCE_OpenSource.md` (§8, §9, §10, §18.3). Este documento refleja el proceso real de `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml` y `tools/install.ps1` en `main`. Reglas de revisión: `AGENTS.md` §5–§6.

## Resumen (camino rápido, PowerShell)

```powershell
git submodule update --init --recursive   # una vez tras clonar
cmake --preset msvc
cmake --build --preset release
ctest --test-dir build --build-config Release --output-on-failure
powershell -File tools\install.ps1        # como administrador
```

---

## 1. Prerrequisitos

| Herramienta | Versión / detalle | Para qué |
|---|---|---|
| Windows | 10/11 x64 | Plataforma soportada (única) |
| Visual Studio 2022 | Community, Professional o Build Tools, con el workload **Desktop development with C++** (MSVC x64 + Windows SDK) | Compilador (`cl.exe`), MSBuild y generador VS |
| CMake | ≥ 3.22 (el mínimo lo fija `CMakeLists.txt`; verificable con `cmake --version`) | Configuración y build |
| Git | reciente, con soporte de submódulos | Clonar y obtener JUCE |
| Ninja | solo si usas el preset `ci` (VS 2022 lo incluye; también se puede instalar aparte) | Generador del preset `ci` |
| JUCE | submódulo fijado en el tag **9.0.5** (commit `d60b7a7`) | Framework del plugin |
| pluginval | 1.0.4, portable (opcional en local; obligatorio en CI y en la definición de hecho) | Validación del VST3 |

VS 2022 instala CMake, `ctest` y Ninja bajo `Common7\IDE\CommonExtensions\Microsoft\CMake\`; si no están en tu `PATH`, usa *Developer PowerShell for VS 2022* o añade esa carpeta manualmente. Una instalación independiente de CMake ≥ 3.22 también vale.

Comprobar el entorno (opcional):

```powershell
cmake --version
git --version
cl          # solo en Developer PowerShell / vcvars
```

JUCE nunca se instala aparte: vive en `JUCE/` como submódulo. Si acabas de clonar:

```powershell
git submodule update --init --recursive
git submodule status          # debe mostrar el commit fijado, no un guion inicial
```

Nunca modifiques ni commitees dentro de `JUCE/` (`AGENTS.md` §6).

---

## 2. Compilación local paso a paso

```powershell
# 1. Clonar (URL real del repositorio)
git clone https://github.com/Oyhs-co/auralis-vst3-ccp
cd auralis-vst3-ccp

# 2. Obtener JUCE (submódulo fijado)
git submodule update --init --recursive

# 3. Configurar (genera la solución VS en build/)
cmake --preset msvc

# 4. Compilar Release
cmake --build --preset release

# 5. (Opcional) Compilar Debug
cmake --build --preset debug
```

La primera configuración compila JUCE por completo y tarda varios minutos; las siguientes son incrementales.

**Rutas exactas de los artefactos** (verificadas en un build real):

| Artefacto | Preset `msvc` (Release) | Preset `msvc` (Debug) | Preset `ci` |
|---|---|---|---|
| Plugin VST3 | `build\Auralis_artefacts\Release\VST3\Auralis.vst3` | `build\Auralis_artefacts\Debug\VST3\Auralis.vst3` | `build-ci\Auralis_artefacts\VST3\Auralis.vst3` |
| Tests | `build\Release\auralis_tests.exe` | `build\Debug\auralis_tests.exe` | `build-ci\auralis_tests.exe` |

`Auralis.vst3` es un **bundle** (carpeta con `Contents\x86_64-win\Auralis.vst3` dentro), no un fichero suelto: cópialo/muévelo siempre completo.

Instalar en el sistema (FL Studio escanea esa carpeta):

```powershell
# Desde una consola con permisos de administrador:
powershell -File tools\install.ps1
# Ruta por defecto: build\Auralis_artefacts\Release\VST3\Auralis.vst3
# Ruta destino:    C:\Program Files\Common Files\VST3\Auralis.vst3
```

Si el plugin se construyó en otra ruta (p. ej. `build-ci`):

```powershell
powershell -File tools\install.ps1 -Vst3Path build-ci\Auralis_artefacts\VST3\Auralis.vst3
```

Después, rescan de plugins en FL Studio. Detalle de instalación y verificación en §7.

---

## 3. Presets de CMake

Tabla de `CMakePresets.json` (solo hay dos presets de configure y tres de build):

| Preset | Generador | `binaryDir` | Config | Build presets | Uso |
|---|---|---|---|---|---|
| `msvc` | Visual Studio 17 2022, x64 | `build/` | `Debug;Release` (multi-config) | `debug`, `release` | Desarrollo local en Windows |
| `ci` | Ninja | `build-ci/` | `Release` (`CMAKE_BUILD_TYPE`) | `ci` | Reproducir exactamente lo que hace CI |

Por qué:

- **`msvc`** usa el generador de Visual Studio: genera solución `.sln`, permite Debug y Release en el mismo directorio y no requiere entorno MSVC en el `PATH`. `cmake --build --preset release` selecciona la configuración Release de esa solución (`COPY_PLUGIN_AFTER_BUILD` está desactivado: el `.vst3` se queda en `build/`, no se copia solo).
- **`ci`** usa Ninja con una sola configuración (`Release`), separado en `build-ci/` para no pisar el build de desarrollo ni sus cachés. Es el preset que ejecuta GitHub Actions, por eso los pasos de CI son `cmake --preset ci` + `cmake --build --preset ci` + `ctest --test-dir build-ci`.
- Para usar `ci` en local necesitas `cl.exe` visible y Ninja en `PATH` (Developer PowerShell for VS 2022). Si no, usa `msvc`: en CI el entorno MSVC lo aporta la action `ilammy/msvc-dev-cmd@v1`.

---

## 4. Tests

Los tests son **DSP puros**: `auralis_tests` enlaza `auralis_dsp_pipeline` y `auralis_analysis`, sin JUCE, sin host y sin GUI (`AGENTS.md` §2). El adaptador VST3 se valida con pluginval (§6).

Se registran en CTest como un único test, `auralis_unit` (`add_test` en `CMakeLists.txt`).

### 4.1 Ejecutar

```powershell
# Con el preset msvc (multi-config: hay que indicar la configuración)
cmake --build --preset release
ctest --test-dir build --build-config Release --output-on-failure

# Debug
cmake --build --preset debug
ctest --test-dir build --build-config Debug --output-on-failure

# Con el preset ci (Ninja, single-config: sin --build-config)
cmake --preset ci; cmake --build --preset ci
ctest --test-dir build-ci --output-on-failure
```

Alternativa: ejecutar el binario directamente. Imprime `ALL TESTS PASSED` y devuelve código 0 si todo pasa; en caso contrario imprime `CHECK failed: <condición> at <archivo>:<línea>` y el número de fallos.

```powershell
build\Release\auralis_tests.exe
```

Solo compilar los tests (sin el plugin): `cmake --build --preset release --target auralis_tests`.

### 4.2 Inventario de tests (`tests/unit/`)

10 ficheros: `TestMain.cpp` (runner + tests de pipeline/gain) y 9 suites por procesador. El runner agrega el contador de fallos de todas las suites y devuelve un único código de salida.

| Fichero | Cubre | Casos clave |
|---|---|---|
| `TestMain.cpp` | `DspPipeline` + `GainProcessor` | Pipeline vacío = passthrough; ganancia 0 dB = passthrough (±1e-6, 3 bloques); +6 dB sobre 0.25 → 0.5 ±2%; bypass y procesador deshabilitado = passthrough; tamaños de bloque variables `{1, 7, 64, 1024, 33}` finitos y acotados; smoothing continuo (primer sample tras un salto de 12 dB entre 0.5 y 1.5, sin salto instantáneo); ±0.5 alternante a 96 kHz sin NaN; ruta mono = passthrough |
| `TestEqualizer.cpp` | EQ de 4 bandas (`EqualizerProcessor`) | Plano = passthrough (1e-4); low shelf +6 dB@100 Hz refuerza 50 Hz (>0.4) y deja 5 kHz intacto ±10%; high shelf +6 dB@8 kHz refuerza 10 kHz y deja 200 Hz ±10%; peaking +12 dB@1 kHz (Q=1) exacto en el centro (0.5 ±10%) y sin efecto a 100 Hz/10 kHz ±15%; banda deshabilitada = passthrough exacto; 4 bandas a +12 dB con ruido: salida finita |
| `TestBass.cpp` | Realce de graves (`BassEnhancerProcessor`) | Defaults = passthrough (1e-4); sub-protection @40 Hz atenúa 25 Hz (<0.25) y deja 200 Hz (>0.4); shelf +6 dB@100 Hz refuerza 80 Hz (>1.3×) y deja 5 kHz ±10%; saturación a 1.2 de entrada: `|s| ≤ 1.05`, finito y con peak ≤ clean +0.05; compensación no sube el nivel con saturación; ruido a settings extremos finito y ≤4 |
| `TestClarity.cpp` | Exciter de claridad (`ClarityProcessor`) | `amount=0` = passthrough exacto (1e-6) aunque presence=6 dB; `amount=1`, presence +6 dB: 6 kHz >0.5 y 100 Hz ≈ 0.25×0.87 ±12%; presence −6 dB atenúa altos frente a 0 dB; ruido acotado ≤2; onda cuadrada fuerte finita ≤4 |
| `TestDynamics.cpp` | Compresor (`CompressorProcessor`) | Defaults (ratio 1, 0 dB makeup) = transparente (1e-3); curva estática t=−20 dB, ratio 4: salida ≈0.1497 ±12%; medidor GR entre 9 y 12 dB; makeup +6 dB (ratio 1) asienta en ~1.0 ±0.03; por debajo de umbral no hay reducción (0.1 ±3%); release recupera unidad (0.01 ±10%); settings extremos con ruido finito ≤8; modo RMS: finito, nivel <0.45 y GR en (0, 24] |
| `TestAmbience.cpp` | Reverb FDN (`AmbienceProcessor`) | `mix=0` = passthrough exacto (1e-6); cola wet viva tras 30 bloques y decayendo (energía[34] < energía[9]), acotada ≤2; decay 0.3 s vs 3 s: la corta decae más; damping 0.9 vs 0.1: la alta deja menos cola; predelay 100 ms (4800 samples @48 k): 8 bloques de silencio exacto y luego señal; decay 10 s con ruido: estable y ≤2 |
| `TestStereo.cpp` | Estéreo M/S (`StereoEnhancerProcessor`) | Defaults = identidad (1e-6); width 0 → L==R (mono-compatible, 1e-6); side +6 dB en anti-fase → 0.5 ±0.015; mid −6 dB → 0.25 ±0.0075; balance −0.5 → L 0.25, R 0.5; correlación +1 (dúo mono), −1 (anti-fase) y <0.3 (ruido incorrelado); entrada mono = passthrough incluso con width 0 y mid −12 dB; width/mid/side a +12 dB con ruido: finito ≤8 |
| `TestLimiter.cpp` | Limitador con lookahead (`LimiterProcessor`) | Latencia declarada = 240 samples @48 k; por debajo de umbral, salida alineada = entrada retrasada L (±1e-3), estéreo **y** mono; con ceiling −6 dB ningún sample supera `10^(-6/20)+0.02`; GR reportado ≥5 dB; bajo umbral GR <0.5 dB; release 50 ms recupera 0.01 ±15% y 1000 ms va más lento; ruido con extreme settings finito y dentro del ceiling |
| `TestFft.cpp` | `Fft` interna (1024) | Tono coherente en el bin 21: pico exacto en 21 y vecinos ≥10× menores; DC constante → bin 0 = 1024±1 y resto ≈0; impulso → todas las magnitudes = 1±1e-3; linealidad `a+b = c` en re/im (1e-3) |
| `TestAnalyzer.cpp` | `SpectrumAnalyzer` (32 bandas log) | Nunca modifica el buffer (comparación exacta); RMS de seno 0.7071 = −6.02 dB ±0.5; peak hold de 0.9 se mantiene y decae lento en 20 bloques de silencio; seno 1 kHz concentrado en una banda (>0.3, exactamente 1 banda caliente, resto a >±4 bandas <0.05); silencio → −100 dB finito y bandas <0.001 |

Cualquier test nuevo va en `tests/unit/Test<Procesador>.cpp`, se registra en `CMakeLists.txt` (fuente + llamada en `TestMain.cpp`) y debe cubrir passthrough, efecto audible, estabilidad/NaN y ruta mono cuando aplique.

---

## 5. CI de GitHub Actions

`.github/workflows/ci.yml` — un único job `CI` en `windows-latest`:

- **Triggers**: `push` solo a `main`; `pull_request` para cualquier rama. Un PR no se mergea con CI en rojo.
- **Checkout**: `actions/checkout@v4` con `submodules: recursive` (el runner no clona JUCE solo).
- **Setup MSVC**: `ilammy/msvc-dev-cmd@v1` carga el entorno de Visual Studio en el runner (necesario para el generador Ninja).
- **Configure**: `cmake --preset ci` → Ninja, `build-ci/`, Release.
- **Build**: `cmake --build --preset ci`.
- **Test**: `ctest --test-dir build-ci --output-on-failure` (sin `--build-config`: es single-config).
- **Validate**: descarga `pluginval_Windows.zip` de la release oficial **v1.0.4** con `Invoke-WebRequest`, lo descomprime en `./pluginval` y ejecuta `./pluginval/pluginval.exe --strictness-level 8 --validate build-ci/Auralis_artefacts/VST3/Auralis.vst3`. Si el plugin no pasa el nivel 8, el job falla.
- **Package**: `Compress-Archive` empaqueta el bundle en `Auralis-win64.zip` y `Get-FileHash -Algorithm SHA256` escribe el hash en `Auralis-win64.zip.sha256`.
- **Upload**: `actions/upload-artifact@v4` publica el artefacto `Auralis-win64` con el ZIP y su `.sha256` (descargable desde la vista del workflow run).

Nota de rutas: CI valida/empaqueta desde `build-ci/Auralis_artefacts/VST3/...` (Ninja no añade subcarpeta `Release/`); tu build local `msvc` está en `build/Auralis_artefacts/Release/VST3/...`.

---

## 6. pluginval

**Qué es**: validador open-source de Tracktion que carga el plugin en un host de prueba y ejecuta automáticamente escaneo e instanciación, guardado/restauración de estado, parámetros y automatización, comportamiento entre hilos y procesado de audio en condiciones variadas. Es el gate de calidad del proyecto para el adaptador VST3 y no se distribuye con el plugin (`THIRD_PARTY_NOTICES.md`).

**Instalación local** (portable, sin instalador) — los mismos comandos que usa CI:

```powershell
Invoke-WebRequest -Uri https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip -OutFile pluginval_Windows.zip
Expand-Archive -Path pluginval_Windows.zip -DestinationPath pluginval
```

**Comando exacto** (equivalente al de CI, para tu build local):

```powershell
./pluginval/pluginval.exe --strictness-level 8 --validate build/Auralis_artefacts/Release/VST3/Auralis.vst3
```

**Strictness 8**: pluginval escala su rigor de 1 a 10; a mayor nivel, más escenarios y más exigentes, y más tiempo. El proyecto exige el nivel 8 (`AGENTS.md` §5, plan §10): un nivel alto, por encima del comportamiento por defecto. No bajes el nivel para “pasar”: si falla, es un bug del plugin.

**Interpretar un fallo**: pluginval devuelve código de salida distinto de 0 y escribe en consola el test que falló (estado, parámetros, audio, hilos…). El mensaje suele señalar la operación concreta —guarda ese log, es la evidencia para el PR. Reproduce el problema, corrígelo y vuelve a ejecutar el comando completo. Guía de diagnóstico detallada: [TROUBLESHOOTING.md](TROUBLESHOOTING.md) y [USER-GUIDE.md](USER-GUIDE.md) para probar en FL Studio.

---

## 7. Distribución e instalación

**Contenido del artefacto de CI** `Auralis-win64`:

- `Auralis-win64.zip` → dentro, únicamente el bundle `Auralis.vst3` (con `Contents\x86_64-win\Auralis.vst3`).
- `Auralis-win64.zip.sha256` → el hash SHA-256 del ZIP, en texto plano (solo el hash).

La release del repositorio (plan §18.3) añade a estos binarios: versión semántica y notas, enlace al tag/commit exactos, código fuente correspondiente publicado, estas instrucciones de compilación, lista de dependencias/licencias y limitaciones conocidas.

**Instalación con `tools/install.ps1`** (pensado para uso desde el repo; requiere **permisos de administrador** porque escribe en `Program Files`):

```powershell
powershell -File tools\install.ps1
```

- Origen por defecto: `build\Auralis_artefacts\Release\VST3\Auralis.vst3` (se puede cambiar con `-Vst3Path`).
- Destino: `$env:COMMONPROGRAMFILES\VST3` → normalmente `C:\Program Files\Common Files\VST3\`.
- Resultado: `C:\Program Files\Common Files\VST3\Auralis.vst3`.
- Tras instalar, rescan de plugins en FL Studio.

**Instalación manual** (p. ej. desde el ZIP de release):

1. Descomprime `Auralis-win64.zip`.
2. Copia la carpeta completa `Auralis.vst3` a `C:\Program Files\Common Files\VST3\` (necesita administrador).
3. Rescan de plugins en FL Studio.

**Verificación del hash** (integridad de la descarga, no firma digital):

```powershell
(Get-FileHash Auralis-win64.zip -Algorithm SHA256).Hash
Get-Content Auralis-win64.zip.sha256
```

Ambos valores deben coincidir. Si no, vuelve a descargar: el ZIP está corrupto o manipulado.

---

## 8. Checklist de release (proceso reproducible)

1. **Versión**: cambia solo la línea `project(Auralis VERSION 0.2.0 LANGUAGES C CXX)` de `CMakeLists.txt`. Convención semántica `MAJOR.MINOR.PATCH`: MINOR para funcionalidad nueva compatible, PATCH para correcciones, MAJOR para cambios incompatibles (mientras sea 0.x, el proyecto está en desarrollo). JUCE propaga la versión del proyecto al plugin; no hay otra línea que tocar. La versión del esquema de presets (`schema_version`) es independiente. Actualiza también el apartado *Status* del `README.md` y prepara las notas.
2. **Build limpio**: borra el directorio de build y compila de cero (`Remove-Item -Recurse -Force build` → `cmake --preset msvc` → `cmake --build --preset release`). Cero warnings nuevos (los flags de warnings de JUCE están activos y no son opcionales).
3. **Tests verdes**: `ctest --test-dir build --build-config Release --output-on-failure` (y la ruta Debug si toca).
4. **pluginval verde**: ejecuta el comando del §6 con `--strictness-level 8` sobre el `.vst3` recién compilado.
5. **Smoke test en host**: FL Studio escanea y carga; prueba mono y estéreo, tamaños de bloque variables, sample rates 44.1/48/88.2/96 kHz y guardado/restauración de estado.
6. **Empaquetado** (misma receta que CI, con la ruta del build local):

   ```powershell
   Compress-Archive -Path build/Auralis_artefacts/Release/VST3/Auralis.vst3 -DestinationPath Auralis-win64.zip
   (Get-FileHash Auralis-win64.zip -Algorithm SHA256).Hash | Out-File Auralis-win64.zip.sha256
   ```

7. **Publicación**: tag de la versión (p. ej. `vX.Y.Z`), notas con la lista del plan §18.3 (versión, hash, tag/commit, código fuente, dependencias, limitaciones) y el ZIP + `.sha256` como artefactos de release.

Regla de oro AGPL: ningún binario se publica sin el código fuente correspondiente accesible en el mismo tag (plan §18.3).

---

## 9. Licencia y dependencias

- El proyecto es **AGPL-3.0-only** (`LICENSE`); cada fichero fuente empieza con `// SPDX-License-Identifier: AGPL-3.0-only`.
- **JUCE 9.0.5** se usa bajo la opción AGPLv3 de su doble licencia (sin licencia comercial), con el submódulo fijado en el commit `d60b7a7`. Nunca se modifica ni se commitea dentro de `JUCE/`.
- El **VST3 SDK** viene dentro de JUCE (licencia tipo MIT, ≥3.8): conserva sus avisos de copyright. “VST” es marca registrada de Steinberg.
- **pluginval 1.0.4** es solo herramienta de CI/validación local: no se empaqueta con el plugin.
- Obligaciones al distribuir binarios: publicar el código fuente correspondiente y las instrucciones de compilación (este documento), la lista de dependencias/licencias (`THIRD_PARTY_NOTICES.md`) y la versión/tag exactos. No existe ni existirá edición cerrada.
- Regla de merge (`AGENTS.md` §6): toda dependencia, asset, icono, sample o preset nuevo necesita su fila en `THIRD_PARTY_NOTICES.md` (origen, versión, licencia, condiciones) **antes** del merge; licencia compatible con AGPL-3.0-only (MIT/BSD sí; incompatible o de procedencia desconocida no).

---

## 10. Errores frecuentes de build

| Síntoma | Causa | Solución |
|---|---|---|
| `add_subdirectory given source "JUCE" which is not an existing directory` o `JUCE/` vacío | Submódulo sin inicializar | `git submodule update --init --recursive` y comprueba con `git submodule status` |
| `Could not find any instance of Visual Studio` / generador `Visual Studio 17 2022` no encontrado | VS 2022 sin instalar o sin el workload C++ | Instala/reaplica **Desktop development with C++** (MSVC x64 + Windows SDK) |
| `cmake` o `ctest` no se reconoce como comando | CMake no está en `PATH` | Usa *Developer PowerShell for VS 2022* o añade la carpeta CMake de VS (o una instalación independiente) al `PATH` |
| `ninja: no such file or directory` al usar `cmake --preset ci` | Ninja no está en `PATH` (y/o faltan `cl.exe` y entorno MSVC) | Usa *Developer PowerShell for VS 2022*, o instala Ninja, o compila con el preset `msvc` |
| Errores raros tras cambiar `CMakeLists.txt`, actualizar el submódulo o cambiar de rama | Directorio de build contaminado/obsoleto | `Remove-Item -Recurse -Force build` (y `build-ci` si aplica) y vuelve a configurar desde cero. No borres `JUCE/` |
| `Access denied` al ejecutar `tools\install.ps1` | `Program Files` requiere elevación | Abre PowerShell **como administrador** |
| FL Studio no encuentra el plugin | Bundle incompleto en la carpeta escaneada o rescan pendiente | Verifica `C:\Program Files\Common Files\VST3\Auralis.vst3\Contents\...` y haz rescan; ver [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |
| Falla la validación con pluginval | Bug real del adaptador VST3/estado/audio | Conserva el log, reproduce con el comando del §6 y corrige; no bajes el strictness |

Comando de limpieza total reproducible:

```powershell
Remove-Item -Recurse -Force build    # y/o build-ci
git submodule update --init --recursive
cmake --preset msvc
cmake --build --preset release
ctest --test-dir build --build-config Release --output-on-failure
```

---

## 11. Notas para contribuir

- **Ramas**: `feature/*`, `fix/*`, `docs/*` (plan §9.1). Abre un issue primero para cambios significativos; los arreglos pequeños van directos a PR.
- **PR**: describe el cambio, la motivación y los tests ejecutados (build, pluginval, mono/estéreo, tamaños de bloque); incluye impacto de CPU/latencia si toca el camino de audio. Se recomienda `git commit -s` (DCO).
- **Definición de hecho** (`AGENTS.md` §5, plan §10):
  1. Build limpio con `cmake --preset msvc` + `cmake --build --preset release`.
  2. Cero warnings nuevos.
  3. pluginval pasa con `--strictness-level 8`.
  4. Mono y estéreo funcionan, con tamaños de bloque variables.
  5. Sin NaN/inf; sample rates 44.1/48/88.2/96 kHz correctos; estado verificable; latencia declarada si aplica.
- **Higiene**: no se commitea `build/` ni artefactos binarios; no se toca `JUCE/`; no se implementa por delante del slice actual; código muerto se borra.

---

## Documentación relacionada

- [ARCHITECTURE.md](ARCHITECTURE.md) — capas, límites y cadena DSP.
- [INTERFACES.md](INTERFACES.md) — contrato de procesadores y pipeline.
- [SHORTCUTS-DEBT.md](SHORTCUTS-DEBT.md) — simplificaciones deliberadas y cuándo revisarlas.
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — diagnóstico de fallos de build, host y pluginval.
- [USER-GUIDE.md](USER-GUIDE.md) — instalación y uso para el usuario final.
- También: `README.md`, `CONTRIBUTING.md`, `AGENTS.md`, `THIRD_PARTY_NOTICES.md` y el plan del proyecto.
