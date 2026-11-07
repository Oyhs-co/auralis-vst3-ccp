# Guía de usuario de Auralis

> Versión documentada: **0.2.0** (estado actual del repositorio). Este documento describe lo que el plugin hace realmente en esta versión: cada control, cada preset y cada comportamiento visible.
>
> Documentación relacionada: [ARCHITECTURE.md](ARCHITECTURE.md) · [PARAMETERS.md](PARAMETERS.md) · [DSP-ALGORITHMS.md](DSP-ALGORITHMS.md) · [TROUBLESHOOTING.md](TROUBLESHOOTING.md) · [BUILD-TEST-RELEASE.md](BUILD-TEST-RELEASE.md)

---

## 1. Qué es Auralis y qué problema resuelve

Auralis es un plugin VST3 **gratuito y de código abierto** para Windows. En una sola ventana reúne una cadena de mejora de audio con un orden fijo:

`Master Gain → EQ → Graves → Claridad → Compresor → Ambiente → Estéreo → Limitador`

El problema que resuelve es concreto: mejorar una mezcla (o cómo suenan tus monitores, tus auriculares o la música en streaming) **sin montar una cadena de seis plugins ni saber de mezcla**. En lugar de configurar cada módulo desde cero, puedes:

- cargar uno de los **presets de fábrica** y listo;
- usar el mando global **Intensity** para decidir "cuánto" efecto quieres, sin tocar nada más;
- comparar dos versiones de tus ajustes con **A/B** antes de decidir;
- mirar el **visor de espectro** para entender qué está cambiando el plugin.

Qué **no** es: no es una suite de mastering, no restaura audio dañado (ruido, clips, graves que faltan), no separa fuentes y no arregla una mala grabación. Es un embellecedor y controlador de mezcla ligero.

Auralis no envía telemetría, no requiere internet, ni cuentas, ni iLok. Funciona en pistas mono y estéreo.

---

## 2. Requisitos

| Necesitas | Detalle |
|---|---|
| Sistema | Windows 10 o 11, **64 bits** |
| DAW | Cualquier DAW 64-bit con soporte VST3. **FL Studio es el host prioritario** (validado con pluginval a strictness 8) |
| Para instalar desde ZIP | Nada más: el ZIP ya trae el plugin compilado |
| Para compilar tú mismo | Visual Studio 2022 con la carga de trabajo *Desktop development with C++*, CMake ≥ 3.22 y Git ([BUILD-TEST-RELEASE.md](BUILD-TEST-RELEASE.md)) |

No hay instalador `.exe`: el plugin es una carpeta `Auralis.vst3` que se copia en la carpeta estándar de VST3 de Windows.

---

## 3. Instalación

### 3.1 Desde el ZIP (recomendado)

Todavía no hay "Releases" publicadas; el ZIP lo genera la integración continua del repositorio:

1. Entra en el repositorio: <https://github.com/Oyhs-co/auralis-vst3-ccp>.
2. Pestaña **Actions** → flujo **CI** → la última ejecución sobre `main`.
3. En **Artifacts**, descarga `Auralis-win64.zip` (incluye un archivo `.sha256` para comprobar la descarga).
4. Descomprime el ZIP: dentro está la carpeta `Auralis.vst3`.
5. Copia **la carpeta completa** `Auralis.vst3` a `C:\Program Files\Common Files\VST3` (Windows pedirá permisos de administrador).

> Si los artefactos de CI no están disponibles (caducan), compila desde el código fuente con [BUILD-TEST-RELEASE.md](BUILD-TEST-RELEASE.md) o el `README.md`.

### 3.2 Instalación automática (si compilas en tu equipo)

Desde la raíz del repositorio, en PowerShell:

```powershell
git submodule update --init --recursive
cmake --preset msvc
cmake --build --preset release
powershell -File tools\install.ps1
```

El script `tools\install.ps1` hace exactamente dos cosas:

- Comprueba que existe el plugin compilado en `build\Auralis_artefacts\Release\VST3\Auralis.vst3` (si no, avisa y termina).
- Copia esa carpeta a `%COMMONPROGRAMFILES%\VST3`, que en un Windows normal es `C:\Program Files\Common Files\VST3`.

Si PowerShell da un error de permisos, ejecútalo como administrador. Puedes indicar otra ruta del plugin compilado con `-Vst3Path <ruta>`.

### 3.3 Instalación manual

1. Copia la carpeta `Auralis.vst3` a `C:\Program Files\Common Files\VST3`.
2. Alternativa sin permisos de administrador: deja la carpeta donde quieras (por ejemplo `C:\VST3\Auralis.vst3`) y añade esa carpeta a las rutas de búsqueda de plugins de tu DAW (en FL Studio: *Options → Manage plugins → Plugin search paths*).

### 3.4 Que FL Studio lo detecte

1. Abre FL Studio.
2. Ve a **Options → Manage plugins**.
3. Pulsa **Find more plugins** (o marca **Verify plugins** para forzar el reanálisis).
4. Cuando termine el escaneo, Auralis aparecerá en el navegador de plugins (fabricante **Auralis**) y podrás cargarlo como efecto en un canal del mixer.

Si no aparece, revisa que la ruta `C:\Program Files\Common Files\VST3` esté en las rutas de búsqueda y que FL Studio sea de 64 bits. Más soluciones en la sección 8 y en [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

### 3.5 Desinstalación

1. Cierra FL Studio (o tu DAW).
2. Borra la carpeta `Auralis.vst3` de `C:\Program Files\Common Files\VST3` (y de cualquier otra carpeta de plugins donde la hayas copiado).
3. Vuelve a escanear plugins en el DAW para que desaparezca de la lista.
4. En los proyectos que ya lo usaban aparecerá como plugin no encontrado; quítalo de esos canales.

---

## 4. La interfaz, control a control

La ventana se abre en **900 × 640** píxeles y se puede redimensionar entre 760 × 540 y 1600 × 1000. Tiene tres zonas: barra superior, visor de espectro y rejilla de parámetros.

### 4.1 Barra superior

De izquierda a derecha:

| Control | Nombre exacto | Qué hace |
|---|---|---|
| Lista | **Preset** | Los presets de fábrica. Al elegir uno aplica sus ajustes; si no has elegido ninguno, muestra la palabra "Preset". |
| Botón | **Save** | Guarda el estado actual de **todos** los parámetros como un archivo `.auralis` (JSON) en la carpeta que elijas. |
| Botón | **Load** | Carga un archivo `.auralis` guardado. Solo aplica los parámetros que contiene el archivo. |
| Botón | **Reset** | Devuelve **todos** los parámetros a sus valores por defecto (estado neutro). No borra la memoria A/B. |
| Botón | **A / B** | Muestra la ranura activa (A o B) y alterna entre las dos. Ver 4.4. |
| Deslizador | **Intensity** | De **0.00 a 1.00** (por defecto **1.00**). Control global de profundidad. Ver 4.3. |
| Casilla | **Bypass** | Activa el parámetro de bypass del plugin. En la versión 0.2.0 **solo anula la etapa Master Gain**: los efectos siguen procesando. Para comparar "con/sin efecto" usa **Intensity = 0.00** o A/B. |

Presets de fábrica (nombres exactos): **Warm Bass**, **Vocal Clarity**, **Wide Stereo**. Aplican solos los parámetros que definen (ver 5.0), así que no resetean el resto de la cadena; si vas a probar varios seguidos, pulsa antes **Reset** para no acumular ajustes.

### 4.2 Visor de espectro

Está debajo de la barra superior y ocupa aproximadamente el 30 % de la altura.

- Muestra **32 barras logarítmicas** entre **20 Hz** (izquierda) y **20 kHz** (derecha).
- **Barras grises** (más anchas): el audio tal como **entra** al plugin.
- **Barras verdeazuladas** (más estrechas, encima): el audio tal como **sale**, después de la cadena completa.
- **Línea amarilla**: el pico más alto reciente de la salida; va bajando poco a poco.
- Pie del visor: **In x dB** (nivel RMS de la entrada), **Out x dB** (nivel RMS de la salida) y **Corr ±x.xx** (correlación estéreo).

Cómo leerlo:

- Si la barra verde de una zona es **más alta** que la gris, el plugin está subiendo esa zona; si es más baja, la está bajando. Es la forma más rápida de ver si un ajuste de EQ, graves o claridad hace lo que crees.
- No hay ejes numéricos ni escala en dB dibujada: sirve para comparar entrada contra salida y para ver la evolución, no como medidor calibrado. Para niveles, usa los valores **In/Out dB**.
- **Corr** va de -1 a +1: +1 significa que izquierda y derecha son idénticas (mono), 0 que el estéreo es muy ancho, y un valor negativo indica problemas de fase (la mezcla puede sonar rara o perder contenido en mono). Mantén Corr por encima de 0 (mejor por encima de +0.3) si la mezcla debe funcionar en mono.
- Cuando el DAW no reproduce audio, el visor se queda quieto con los últimos valores: es normal, no es un cuelgue.

### 4.3 Intensity (control global)

**Intensity** multiplica la profundidad de los efectos: 0.00 deja la cadena de efectos neutral y 1.00 aplica los ajustes tal como los pusiste.

- **Sí escala**: ganancias del EQ, ganancia y saturación de graves, cantidad y presencia de claridad, ratio y makeup del compresor, mezcla del ambiente y anchura estéreo.
- **No toca**: frecuencias, Q, umbral ni tiempos del compresor, predelay/decay/damping/anchura del reverb, Balance, Bass Sub HP, **Master Gain** ni el **limitador** (es una etapa de seguridad y siempre actúa igual).

Truco: deja los módulos ajustados "a tu gusto fuerte" y baja Intensity hasta que suene sutil. Así un mismo preset sirve para pista, bus o master.

### 4.4 A/B

El botón **A/B** guarda y alterna entre dos memorias de ajustes completos:

- El primer clic copia el estado actual a la otra ranura (no hay salto de sonido: A y B empiezan iguales).
- A partir de ahí, edita en la ranura que muestra el botón; al pulsarlo se guarda lo tuyo y se carga lo de la otra ranura.
- Ambas ranuras se guardan con el proyecto del DAW (y con el estado del plugin).

Úsalo para decidir entre "sutil vs fuerte", "con aire vs sin aire", etc., siempre escuchando al mismo volumen (ajusta el fader, no el volumen del plugin, para que la comparación sea justa).

### 4.5 Rejilla de parámetros

Debajo del visor hay una rejilla desplazable de mandos giratorios (6 por fila), **en el mismo orden que la cadena de audio**: EQ, Graves, Claridad, Compresor, Ambiente, Estéreo y Limitador, con el valor numérico debajo de cada mando. Cada módulo empieza con sus ajustes neutros; activa solo lo que necesites.

#### EQ (ecualizador, 4 bandas fijas)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **EQ Low Freq** | 20 – 500 Hz | 100 Hz | Frecuencia de la banda baja (shelving). |
| **EQ Low Gain** | -15 – +15 dB | 0 dB | Sube o baja todo lo que está por debajo de esa frecuencia. |
| **EQ Band1 Freq** | 200 – 2000 Hz | 400 Hz | Centro de la primera banda paramétrica. |
| **EQ Band1 Gain** | -15 – +15 dB | 0 dB | Sube o baja esa zona (barro, cuerpo). |
| **EQ Band1 Q** | 0.1 – 10 | 0.8 | Estrechez de la banda: bajo = ancha y musical, alto = estrecha y quirúrgica. |
| **EQ Band2 Freq** | 1000 – 10000 Hz | 2500 Hz | Centro de la segunda banda paramétrica. |
| **EQ Band2 Gain** | -15 – +15 dB | 0 dB | Sube o baja esa zona (dureza, sibilancia, detalle). |
| **EQ Band2 Q** | 0.1 – 10 | 0.8 | Igual que Band1 Q. |
| **EQ High Freq** | 2000 – 20000 Hz | 8000 Hz | Frecuencia de la banda alta (shelving). |
| **EQ High Gain** | -15 – +15 dB | 0 dB | Aire/brillo por encima de esa frecuencia. |

Las bandas shelving (Low y High) no tienen Q ajustable.

#### Graves (Bass)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Bass Freq** | 40 – 200 Hz | 100 Hz | Frecuencia del shelving de graves. |
| **Bass Gain** | -12 – +12 dB | 0 dB | Cantidad de graves añadida o quitada. |
| **Bass Saturation** | 0 – 1 | 0 | Añade armónicos suaves a los graves (se oyen mejor en altavoces pequeños). |
| **Bass Sub HP** | 0 – 60 Hz | 0 (desactivado) | Filtro que elimina lo que está por debajo (protección de subgraves). Un valor de 0 lo apaga; prueba 25–40 Hz si tus altavoces retumban. |

#### Claridad (Clarity)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Clarity Amount** | 0 – 1 | 0 | Cuánta excitación armónica se añade. |
| **Clarity Freq** | 1000 – 8000 Hz | 3000 Hz | Zona sobre la que actúa la claridad. |
| **Clarity Presence** | -6 – +6 dB | 0 dB | Realce o atenuación de la parte añadida. |

#### Compresor (Dynamics)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Comp Threshold** | -60 – 0 dB | 0 dB | Nivel a partir del cual empieza a comprimir. Más abajo = más compresión. |
| **Comp Ratio** | 1 – 20 | 1 | Cuánto se reduce lo que pasa del umbral. 1 = sin compresión; 3–4 = pegado suave; 8 o más = control fuerte. |
| **Comp Attack** | 1 – 100 ms | 10 ms | Cuánto tarda en reaccionar. Rápido = controla picos; más lento = deja pasar transientes. |
| **Comp Release** | 10 – 1000 ms | 150 ms | Cuánto tarda en soltar. Rápido = más natural en graves; lento = bombeo si te pasas. |
| **Comp Knee** | 0 – 24 dB | 6 dB | Suavidad de la transición en el umbral: más knee = más transparente. |
| **Comp Makeup** | 0 – 24 dB | 0 dB | Volumen devuelto después de comprimir. |
| **Comp RMS** (casilla) | on/off | off | Desactivado = detector de picos (controla transientes); activado = detector RMS (compresión más suave y "pegada"). |

El compresor es estéreo-enlazado: una sola reducción de ganancia para los dos canales, así no se desequilibra la imagen.

#### Ambiente (Ambience)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Ambience Mix** | 0 – 1 | 0 | Cantidad de reverb (mezcla seco/húmedo). |
| **Ambience Predelay** | 0 – 200 ms | 20 ms | Retardo antes de la reverb: separa la fuente de la cola. |
| **Ambience Decay** | 0.1 – 10 s | 1.5 s | Duración de la cola. |
| **Ambience Damping** | 0 – 1 | 0.5 | Oscurece la cola a medida que sube. |
| **Ambience Width** | 0 – 1 | 0.7 | Anchura del estéreo del reverb (0 = mono). |

#### Estéreo (Stereo)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Stereo Width** | 0 – 2 | 1 | 0 = mono, 1 = original, 2 = muy ancho. |
| **Stereo Mid Gain** | -12 – +12 dB | 0 dB | Volumen del centro (voz, bombo, bajo). |
| **Stereo Side Gain** | -12 – +12 dB | 0 dB | Volumen de los laterales (lo "ancho"). |
| **Stereo Balance** | -1 – +1 | 0 | 0 = centro; **valores negativos bajan el canal izquierdo** (-1 = solo derecha) y positivos bajan el derecho (+1 = solo izquierda). |

Vigila **Corr** en el visor al ensanchar. En una pista mono, este módulo deja pasar la señal sin cambios.

#### Limitador (Limiter)

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **Limiter Threshold** | -24 – 0 dB | 0 dB | Nivel a partir del cual empieza a limitar. Bájalo a -2/-1 para controlar picos de master. |
| **Limiter Ceiling** | -12 – 0 dB | 0 dB | Techo duro de salida: nada sale por encima. -1 dB deja margen para los códecs de streaming. |
| **Limiter Release** | 5 – 1000 ms | 50 ms | Velocidad con la que recupera tras un pico. Corto = más fuerte y natural; largo = más compacto. |

El limitador usa 5 ms de lookahead (mira 5 ms hacia delante para atrapar el pico antes de que salga) y **declara esa latencia al DAW**, que la compensa. Es la última etapa de la cadena.

### 4.6 Parámetros sin control en la ventana

En esta versión hay un parámetro que **no tiene mando visible** en la interfaz:

- **Master Gain** (rango -60 – +12 dB, por defecto 0 dB): ajuste de ganancia de entrada/salida del plugin. Se puede ajustar y automatizar desde la lista de parámetros del DAW, pero no hay mando en la ventana en la versión 0.2.0.

Todos los controles de Auralis son parámetros del host: se pueden automatizar y se guardan con el proyecto del DAW (incluidos A/B y el estado del plugin).

---

## 5. Guía de escucha: empieza aquí

Reglas de oro para todas las recetas:

- Compara siempre **al mismo volumen** (ajusta el fader del DAW, no el efecto). Un ajuste "mejor" que suena más alto suele ser solo más alto.
- Movimientos de **1–3 dB** en EQ y pasos pequeños en el resto: Auralis embellece, no reescribe.
- Empieza por **Intensity**, luego afina los módulos. Prueba con A/B antes de decidir.

### 5.0 Qué hacen los presets de fábrica (por si quieres hacerlos a mano)

| Preset | Ajustes que aplica |
|---|---|
| **Warm Bass** | Bass Freq 90 Hz, Bass Gain +5 dB, Bass Saturation 0.25, EQ Low Freq 90 Hz, EQ Low Gain +2 dB |
| **Vocal Clarity** | Clarity Amount 0.45, Clarity Freq 3200 Hz, Clarity Presence +2 dB, EQ Band1 250 Hz / -2 dB / Q 1.2, EQ Band2 3000 Hz / +2 dB, Comp Threshold -18 dB, Comp Ratio 3, Comp Makeup +2 dB |
| **Wide Stereo** | Stereo Width 1.4, Ambience Mix 0.15, Ambience Decay 1.2 s, EQ High Gain +1.5 dB |

Recuerda: un preset **no resetea** el resto de la cadena.

### 5.1 Pista individual (voz, guitarra, synth solista)

1. Carga **Vocal Clarity** y baja **Intensity** a 0.6–0.8.
2. Ajuste fino:
   - Si hay barro: **EQ Band1 Gain** -2 a -4 dB con **EQ Band1 Freq** entre 200 y 300 Hz.
   - Si hay dureza o sibilancia: **EQ Band2 Gain** -2 a -3 dB con **EQ Band2 Freq** entre 5000 y 8000 Hz.
   - Para que destaque: **Clarity Amount** 0.3–0.5 y **Clarity Presence** +1 a +3 dB.
   - Control de dinámica: **Comp Threshold** -15 a -20 dB, **Comp Ratio** 2–3, **Comp RMS** activado si es voz.
   - Aire: **EQ High Gain** +1 a +2 dB con **EQ High Freq** 8000–12000 Hz.
3. Comprueba en el visor que las barras de salida cambian donde esperas y compara con A/B.

### 5.2 Bus (batería, synths, coros)

1. Limpia antes con **Reset** (si venías de un preset) y baja **Intensity** a 0.5–0.8.
2. Ajuste fino:
   - Pegamento: **Comp Threshold** -18 dB, **Comp Ratio** 2–4, **Comp Attack** 10–30 ms, **Comp Release** 100–250 ms, **Comp Knee** 6–12, **Comp Makeup** +2 a +4 dB (o **Comp RMS** activado para suavizar).
   - Ancho: **Stereo Width** 1.1–1.3 (no pases de 1.3 sin comprobar Corr).
   - Espacio: **Ambience Mix** 0.05–0.15 con **Ambience Predelay** 15–30 ms.
3. Escucha el bus solo y dentro de la mezcla: el mismo ajuste puede funcionar solo y no en contexto.

### 5.3 Master / música para streaming

1. **Intensity** a 0.5–0.8 (menos es más en el master).
2. Ajuste fino:
   - **Limiter Threshold** -2 a -1 dB (no lo dejes a 0: que atrape los picos) y **Limiter Ceiling** -1 dB (margen para los códecs).
   - **Limiter Release** 50–150 ms para que respire.
   - Aire sutil: **EQ High Gain** +0.5 a +1.5 dB con **EQ High Freq** 8000–12000 Hz.
   - Ancho: **Stereo Width** 1.0–1.15 y comprueba **Corr** > 0.
3. Escucha a volumen bajo y en mono (si el DAW lo permite): es donde se notan los excesos.

### 5.4 Hip-hop / electrónica / música con graves

1. Carga **Warm Bass** y baja **Intensity** a 0.7–0.9.
2. Ajuste fino:
   - Más peso: **Bass Gain** +2 a +4 dB y **EQ Low Gain** +1 a +3 dB con **EQ Low Freq** 80–100 Hz.
   - Textura: **Bass Saturation** 0.15–0.35 (más que eso suena a distorsión).
   - Si tus altavoces retumban o el limitador trabaja demasiado: **Bass Sub HP** 25–40 Hz (recuerda que este control no lo escala Intensity).
   - Claridad sobre los graves: **Clarity Amount** 0.2–0.35 con **Clarity Freq** 3000–4000 Hz.
3. Compara **In/Out dB** en el visor: si la salida sube mucho respecto a la entrada, estás añadiendo volumen además de graves; baja **Bass Gain** o **EQ Low Gain**.

### 5.5 Contenido con voz (podcast, directos, locución)

1. **Intensity** a 0.7 y, si quieres partir de cero, ajusta a mano:
   - **Comp Threshold** -18 dB, **Comp Ratio** 3, **Comp RMS** activado, **Comp Makeup** +2 dB.
   - **Clarity Amount** 0.3–0.45 con **Clarity Freq** 3200 Hz y **Clarity Presence** +1 a +2 dB.
   - **EQ Band1** 250 Hz / -2 dB para quitar barro.
   - **Limiter Threshold** -3 dB y **Limiter Ceiling** -1 dB.
2. La voz pide estabilidad, no brillo: si suena "brillante" o cansada, baja **Clarity Presence** antes que el **Clarity Amount**.

### 5.6 Escuchar en auriculares

1. **Intensity** a 0.5–0.7 y cuidado con el estéreo: en auriculares todo parece más ancho de lo que es.
2. Ajuste fino:
   - **Stereo Width** 1.0–1.2 como máximo.
   - **Ambience Mix** 0.05–0.15; si suena "enlatado" o lejano, bájalo.
   - Si el centro se desdibuja: **Stereo Mid Gain** +0.5 a +1.5 dB o **Stereo Side Gain** -1 a -2 dB.
3. Comprueba **Corr** y escucha también en altavoces: la decisión final se toma en ambos.

### 5.7 Monitores vs streaming (la misma mezcla, dos destinos)

- **Monitores**: puedes permitirte un poco más de aire (**EQ High Gain** +1 a +2 dB) y anchura (**Stereo Width** 1.1–1.3), pero verifica en un altavoz pequeño o en el móvil antes de darlo por bueno.
- **Streaming**: prioriza **Limiter Ceiling** -1 dB, poca compresión (**Comp Ratio** ≤ 3) y nada de limitar fuerte. Las plataformas normalizan el volumen: hacerlo "más fuerte" no te da ventaja y sí quita dinámica.

---

## 6. Cómo suena cada módulo (de oído)

Detalle técnico de cada algoritmo en [DSP-ALGORITHMS.md](DSP-ALGORITHMS.md). Aquí, qué esperar al moverlos.

- **Master Gain** (primera etapa, sin mando en la ventana): volumen general del plugin. No cambia el carácter.
- **EQ**: el control más predecible. Low shelf = peso o fofedad; Band1 = barro (200–400 Hz) o cuerpo; Band2 = dureza (2–4 kHz), sibilancia (5–8 kHz) o detalle; High shelf = aire/brillo (8–12 kHz). Q baja suena musical y amplia; Q alta suena a cirugía. Sirve para tallar lo que molesta más que para "mejorar" por defecto.
- **Graves**: más peso y cuerpo **sin que el nivel se dispare** (compensa internamente). Con **Bass Saturation** aparecen armónicos que hacen que los graves se "lean" en altavoces pequeños; por encima de 0.4 el efecto se vuelve agresivo en la mayoría de mezclas. **Bass Sub HP** limpia retumbe y libera headroom en el limitador.
- **Claridad**: excitador armónico paralelo. Suena a presencia, aire, letra más inteligible, guitarras y hi-hats más definidos. En exceso: arenilla, sibilancias y fatiga auditiva (más evidente con auriculares brillantes).
- **Compresor**: une la señal y controla la dinámica. Peak = controla picos y transientes; RMS = nivel más estable y suave. Suena a mezcla más "pegada" y voz firme; en exceso bombea, achata y quita vida. Empieza por ratio bajo y umbral alto (menos compresión) e incrementa.
- **Ambiente**: reverb FDN con pre-delay, decay, damping y anchura. Un toque (mix 0.05–0.15) da espacio y pegamento; mucho suena a "todo en la iglesia". **Predelay** separa la fuente (más = voz más al frente), **Damping** oscurece la cola, **Decay** controla el tamaño. El DAW conoce la duración de la cola para renders.
- **Estéreo**: anchura y reparto centro/lados. **Stereo Width** >1 abre, <1 acerca a mono; **Mid Gain** realza o hunde el centro; **Side Gain** hace lo mismo con los laterales; **Balance** inclina a un lado. Suena a "más grande" o "más enfocado"; en exceso el centro se debilita y la mezcla pierde compatibilidad mono (mira **Corr**).
- **Limitador**: la red de seguridad final. Con los valores por defecto (threshold 0, ceiling 0) solo evita que nada pase de 0 dBFS. Bajando **Threshold** a -1/-2 controla picos del master; el **Ceiling** es un techo duro, así que si oyes distorsión digital es porque la señal ya está pegada al techo: sube el ceiling o baja lo que le llega. No suena a nada si no actúa. Latencia: 5 ms, compensados por el DAW.

---

## 7. Buenos usos y malos usos

**Buenos usos**

- Colocarlo **al final** de la cadena de la pista, del bus o del master: su limitador ya protege la salida.
- Usar **Intensity** como mando macro antes de retocar módulos.
- Empezar por un preset de fábrica y afinar de oído.
- Decidir con **A/B** y al mismo volumen.
- Comprobar **Corr** y escuchar en mono antes de dar por bueno un ensanchado.
- Guardar tus ajustes como `.auralis` con **Save** y reutilizarlos con **Load** en otras sesiones.

**Malos usos**

- **Doble limitación**: Auralis + otro limitador al final del master. Si de verdad necesitas otro limitador después, sube el **Limiter Threshold** de Auralis a 0 dB (deja que solo proteja) o no lo uses para eso.
- Usarlo como **herramienta restaurativa**: no quita ruido, no arregla clips, no rellena graves que no existen.
- Abusar de **Stereo Width** (>1.4) o de **Side Gain** alto: el centro se debilita y la mezcla se rompe en mono.
- Subir **Bass Saturation** para "sonar más fuerte": añade distorsión audible.
- Confiar en **Bypass** para comparar en la versión 0.2.0: solo anula Master Gain. Usa **Intensity 0.00** o A/B.
- Encadenar presets sin **Reset**: los ajustes se acumulan y ya no sabrás qué estás escuchando.
- Usar **Clarity** para domar sibilancia: para eso, corta con **EQ Band2**.
- Limitar fuerte "para que suene como un hit": quita dinámica; el volumen percibido se gana en la mezcla, no con más limitador.

---

## 8. Preguntas frecuentes y problemas comunes

Para problemas avanzados (logs, versiones, pasos de reproducción), consulta [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

**No aparece en FL Studio.**
Comprueba que la carpeta `Auralis.vst3` está en `C:\Program Files\Common Files\VST3` y que esa ruta está en *Options → Manage plugins → Plugin search paths*. Pulsa **Find more plugins** o **Verify plugins**, y reinicia FL Studio. Asegúrate de usar FL Studio de 64 bits.

**La ventana parece congelada o el visor no se mueve.**
Si el transporte está parado, el visor se queda con los últimos valores: es normal. Si la ventana no responde con el audio sonando, cierra y vuelve a abrir el editor del plugin; si se repite, actualiza a la última versión y revisa [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

**Lo activo y suena igual.**
Los valores por defecto son neutros a propósito. Comprueba: (1) **Intensity** > 0.00, (2) **Bypass** desactivado, (3) que los módulos tengan ajustes (un preset solo toca unos parámetros). Sube **Intensity** a 1.00 y prueba **Warm Bass** o mueve **EQ Low Gain** +4 dB: debe notarse en el visor y en el oído.

**Suena raro, distorsiona o pierde fuerza.**
Causas típicas: **Limiter Ceiling** demasiado bajo para la señal (sube a -1 o 0 dB), **Bass Saturation** alto, **Comp Ratio** alto con **Attack** muy rápido (bombeo), o **Stereo Width** excesivo con **Corr** bajo. Pulsa **Reset** y añade un módulo cada vez para encontrar el culpable.

**Hay latencia.**
El limitador aporta 5 ms de lookahead y el plugin los declara al DAW, que debe compensarlos automáticamente. Si notas retardo al tocar en directo, revisa la compensación de latencia del DAW y usa un buffer pequeño si tu equipo lo permite; los 5 ms del limitador forman parte del diseño.

**Consume mucha CPU o hay chasquidos.**
La cadena es ligera; el limitador y el analizador son las partes más costosas. Si ves picos, aumenta el tamaño de buffer del DAW (por ejemplo de 128 a 256/512 samples). Si el problema persiste, revisa [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

**¿Dónde está el mando Master Gain?**
No tiene control en la ventana en la versión 0.2.0. Puedes ajustarlo o automatizarlo desde la lista de parámetros del DAW.

**¿El plugin guarda mis ajustes?**
Sí: el estado se guarda con el proyecto del DAW, incluidos los A/B. Los presets de usuario son archivos `.auralis` aparte: guárdalos con **Save** y recupéralos con **Load**.

**¿Envía datos a internet?**
No. No hay telemetría, ni cuentas, ni conexión necesaria en ningún momento.

---

## 9. Glosario

| Término | Significado |
|---|---|
| **dB (decibelio)** | Unidad relativa de nivel. +6 dB equivale a duplicar la amplitud; +10 dB suele percibirse como "el doble de volumen". |
| **Hz / kHz** | Frecuencia en hercios / kilohercios (1000 Hz). Graves: ~20–250 Hz; medios: 250 Hz–4 kHz; agudos: 4–20 kHz. |
| **Q** | Estrechez de una banda de EQ: Q alto = banda estrecha, Q bajo = banda ancha. |
| **Compresión** | Reducir automáticamente la diferencia entre las partes fuertes y las flojas de la señal. |
| **Bypass** | Dejar pasar la señal sin procesar. En Auralis 0.2.0, el botón Bypass solo anula Master Gain (ver 4.1). |
| **A/B** | Dos memorias de ajustes para comparar dos versiones de tu sonido sin perder ninguna. |
| **Sample rate** | Frecuencia de muestreo del DAW, en muestras por segundo (44.1, 48, 88.2, 96 kHz...). Auralis funciona en todos esos valores. |
| **VST3** | Formato estándar de plugin (Steinberg) que cargan los DAW. En Windows, los plugins viven en `C:\Program Files\Common Files\VST3`. |
| **Limitador** | Procesador que impide que la señal supere un techo. Auralis lo lleva al final de la cadena. |
| **RMS** | Medida de nivel promedio; al activar **Comp RMS**, el compresor mira el nivel medio en lugar de los picos. |
| **Correlación (Corr)** | Grado en que los canales izquierdo y derecho coinciden. +1 = mono; negativo = problemas de fase. |

---

## 10. Enlaces y soporte

- **Repositorio:** <https://github.com/Oyhs-co/auralis-vst3-ccp>
- **Licencia:** AGPL-3.0-only (ver [LICENSE](../LICENSE)). En una frase: el plugin es gratis y su código es público; si distribuyes el plugin o una versión modificada, debes publicar también el código fuente bajo la misma licencia. Tu música no se ve afectada.
- **Reportar un bug:** <https://github.com/Oyhs-co/auralis-vst3-ccp/issues> — incluye versión de Windows, DAW (y su versión), versión de Auralis, los pasos para reproducirlo y qué esperabas que pasara. Para fallos de seguridad, sigue [SECURITY.md](../SECURITY.md).
- **Documentación interna:** [TROUBLESHOOTING.md](TROUBLESHOOTING.md) · [PARAMETERS.md](PARAMETERS.md) · [DSP-ALGORITHMS.md](DSP-ALGORITHMS.md) · [ARCHITECTURE.md](ARCHITECTURE.md) · [BUILD-TEST-RELEASE.md](BUILD-TEST-RELEASE.md) · [PRODUCT.md](PRODUCT.md)
