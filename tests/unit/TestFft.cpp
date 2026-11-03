// SPDX-License-Identifier: AGPL-3.0-only
// Self-contained checks for auralis::Fft; run_FftTests() returns the failure count.

#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

constexpr double kPi = 3.14159265358979323846;

static float magnitude (const std::vector<float>& re, const std::vector<float>& im, int bin)
{
    return std::sqrt (re[bin] * re[bin] + im[bin] * im[bin]);
}

static void sinePeakInRightBin()
{
    const auralis::Fft fft (1024);
    std::vector<float> re (1024), im (1024, 0.0f);
    for (int i = 0; i < 1024; ++i)
        re[i] = (float) std::sin (2.0 * kPi * (21.0 * 48000.0 / 1024.0) * (double) i / 48000.0);
    fft.transform (re.data(), im.data());

    int maxBin = 0;
    for (int k = 1; k < 512; ++k)
        if (magnitude (re, im, k) > magnitude (re, im, maxBin))
            maxBin = k;

    // A coherent 21st-bin tone should peak exactly at bin 21 without leakage.
    CHECK (maxBin == 21);
    // Neighbor bins should be much smaller
    CHECK (magnitude (re, im, maxBin) > 10.0f * magnitude (re, im, (maxBin + 1) % 512));
    CHECK (magnitude (re, im, maxBin) > 10.0f * magnitude (re, im, (maxBin + 511) % 512));
}

static void dcAndNyquist()
{
    // Constant 1.0 -> DC only
    auralis::Fft fft (1024);
    std::vector<float> re (1024, 1.0f), im (1024, 0.0f);
    fft.transform (re.data(), im.data());
    CHECK (std::fabs (re[0] - 1024.0f) <= 1.0f); // DC bin
    for (int k = 1; k < 512; ++k)
        CHECK (std::fabs (re[k]) <= 1e-3f && std::fabs (im[k]) <= 1e-3f);

    // Impulse at 0 -> all bins = 1
    std::fill (re.begin(), re.end(), 0.0f);
    re[0] = 1.0f;
    std::fill (im.begin(), im.end(), 0.0f);
    fft.transform (re.data(), im.data());
    for (int k = 0; k < 512; ++k)
        CHECK (std::fabs (magnitude (re, im, k) - 1.0f) <= 1e-3f);
}

static void linearity()
{
    auralis::Fft fft (1024);
    std::vector<float> a_re (1024), a_im (1024), b_re (1024), b_im (1024), c_re (1024), c_im (1024);
    for (int i = 0; i < 1024; ++i)
    {
        a_re[i] = (i % 7 == 0) ? 1.0f : 0.0f;
        b_re[i] = (i % 11 == 0) ? 1.0f : 0.0f;
        c_re[i] = a_re[i] + b_re[i];
    }
    fft.transform (a_re.data(), a_im.data());
    fft.transform (b_re.data(), b_im.data());
    fft.transform (c_re.data(), c_im.data());

    for (int k = 0; k < 1024; ++k)
    {
        CHECK (std::fabs (a_re[k] + b_re[k] - c_re[k]) <= 1.0e-3f);
        CHECK (std::fabs (a_im[k] + b_im[k] - c_im[k]) <= 1.0e-3f);
    }
}

// run_FftTests in global namespace
int run_FftTests()
{
    sinePeakInRightBin();
    dcAndNyquist();
    linearity();
    return failures;
}
