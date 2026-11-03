// SPDX-License-Identifier: AGPL-3.0-only

#include "Fft.h"

#include <cmath>
#include <cstddef>

namespace auralis
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
}

Fft::Fft (int size)
    : n (size > 0 && (size & (size - 1)) == 0 ? size : 0)
{
    // A non power-of-two size degrades to a no-op transform instead of throwing.
    if (n == 0)
        return;

    int bits = 0;
    while ((1 << bits) < n)
        ++bits;

    rev.resize ((std::size_t) n);
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if ((i >> b) & 1)
                r |= 1 << (bits - 1 - b);
        rev[(std::size_t) i] = r;
    }

    cosTable.resize ((std::size_t) (n / 2));
    sinTable.resize ((std::size_t) (n / 2));
    for (int k = 0; k < n / 2; ++k)
    {
        const double phase = 2.0 * kPi * (double) k / (double) n;
        cosTable[(std::size_t) k] = (float) std::cos (phase);
        sinTable[(std::size_t) k] = (float) std::sin (phase);
    }
}

void Fft::transform (float* re, float* im) const noexcept
{
    if (n == 0)
        return;

    for (int i = 0; i < n; ++i)
    {
        const int j = rev[(std::size_t) i];
        if (j > i)
        {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }

    // Twiddle exp(-2*pi*i*k/len) reuses the n/2 table at index k * (n / len).
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1;
        const int step = n / len;
        for (int base = 0; base < n; base += len)
        {
            for (int k = 0; k < half; ++k)
            {
                const auto t = (std::size_t) (k * step);
                const float wr = cosTable[t];
                const float wi = -sinTable[t];
                const float xr = re[base + k + half];
                const float xi = im[base + k + half];
                const float tr = xr * wr - xi * wi;
                const float ti = xr * wi + xi * wr;
                re[base + k + half] = re[base + k] - tr;
                im[base + k + half] = im[base + k] - ti;
                re[base + k] += tr;
                im[base + k] += ti;
            }
        }
    }
}
}
