# Auralis — DSP algorithms

Reference for every processor in the fixed chain
`Gain → EQ → Bass → Clarity → Dynamics → Ambience → Stereo → Limiter`.
Each processor implements the `auralis::IDspProcessor` contract (`prepare` / `process` / `reset`)
and reads its parameters once per block through `noexcept` setters; audible values are
smoothed internally. Source of truth is the code; this document is the map.

## Gain (`modules/dsp/gain`)

Master gain with 20 ms one-pole smoothing (dB → linear) and a bypass flag that short-circuits
the stage. Direct multiplication per sample, no filtering.

## Equalizer (`modules/dsp/equalizer`)

Four RBJ biquad bands (direct form I, double-precision coefficients):
low shelf → peaking → peaking → high shelf. Shelves use Butterworth Q = 1/√2 (cookbook
shelf slope S = 1); peaking bands expose their own Q. `setBand` only marks the band dirty
when a value actually changes; dirty bands recompute target coefficients once per block and
per-sample lerp the five coefficients toward them (5 ms) — automation is zipper-free and the
identity at 0 dB is bit-exact. Disabled bands skip filtering entirely.

## Bass Enhancer (`modules/dsp/bass`)

Sub protection (optional, 0 = off): two cascaded RBJ highpass biquads — 24 dB/oct,
−6 dB at the corner (LR4-style). Low shelf (RBJ, gain smoothed in the linear domain).
Optional soft saturation: normalized `tanh(k·x)/tanh(k)` with drive k = 1 + 9·amount,
mixed dry/wet by the amount. Level compensation: `1/(1 + 0.25·sat·max(0, gainDb/12))`.

## Clarity (`modules/dsp/clarity`)

Parallel harmonic exciter: `out = comp·(dry + amount·presence·softClip(highpass(dry)))` with
`softClip(y) = tanh(2y)/tanh(2)`, an RBJ highpass at the center frequency (Butterworth Q),
`comp = 1/(1 + 0.15·amount)`. Amount 0 short-circuits to exact dry.

## Dynamics (`modules/dsp/dynamics`)

Feed-forward compressor, stereo-linked (detected on the loudest channel). Peak or RMS
(one-pole on x², ~10 ms) detector; dB-domain gain computer with quadratic soft knee;
attack/release one-pole smoothing of the applied gain (dB). Ratio and knee are smoothed
(20 ms) and ratio is clamped ≥ 1 per sample. Gain reduction is exposed per block as a
positive-dB atomic. Transparent at defaults (threshold 0 dB, ratio 1).

## Ambience (`modules/dsp/ambience`)

Eight-line feedback delay network: prime delay lengths scaled by sample rate, Householder
feedback (out_i = s_i − ¼·Σs), per-line t60 gains (`g = 10^(−3·loopTime/decay)`) and one-pole
damping (fc = 18000·10^(−damping)). Input: mono sum → two fixed one-sample allpass
diffusers → predelay ring (0–200 ms). Stereo wet buses with width (mid/side) and a linear
dry/wet mix (mix 0 is exact dry; the network keeps running so un-muting is seamless).
Tail length is reported to the host.

## Stereo Enhancer (`modules/dsp/stereo`)

M/S encode-decode: `L' = mid·midGain + side·sideGain·width`, `R' = mid·midGain − side·sideGain·width`
(mid = (L+R)/2, side = (L−R)/2), then linear balance gains (gL = 1+balance for balance ≤ 0,
gR = 1−balance for balance ≥ 0). Width, mid/side gains are smoothed (20 ms); balance is
smoothed (20 ms). Per-block correlation meter `Σ L·R / √(Σ L²·Σ R²)` exposed as an atomic.
Mono input is exact passthrough.

## Output Limiter (`modules/dsp/limiter`)

Lookahead peak limiter: 5 ms per-channel delay line (latency declared to the host) and one
shared gain envelope. The window peak (last L inputs + current, per channel, max across
channels) is computed with an exact sliding-window maximum (monotonic queue, O(1) amortized
per sample). Attack is instant (the lookahead exposes peaks before they reach the output);
recovery is a one-pole release. Ceiling is a hard safety clamp. Gain reduction is exposed
per block as a positive-dB atomic.

## Analysis (`modules/analysis`)

`Fft`: iterative radix-2 Cooley–Tukey, in-place, precomputed twiddles and bit-reversal.
`SpectrumAnalyzer`: read-only observer. Mono sum → ring buffer (always the most recent 1024
samples) → Hann window → FFT every 2nd block → 32 log-spaced band magnitudes (20 Hz–20 kHz)
with exponential moving average (0.7/0.3), published as relaxed atomics alongside RMS, peak
and peak-hold meters. Never modifies audio, never locks.
