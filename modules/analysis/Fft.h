// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <vector>

namespace auralis
{
// Iterative radix-2 Cooley-Tukey forward FFT, convention exp(-2*pi*i*k/N).
// All tables are built in the constructor; transform() is allocation-free.
class Fft
{
public:
    explicit Fft (int size);

    // In-place forward transform of the real and imaginary parts.
    void transform (float* re, float* im) const noexcept;

    int size() const noexcept { return n; }

private:
    int n = 0;                   // 0 marks an invalid (non power-of-two) size
    std::vector<int> rev;        // bit-reversal permutation table
    std::vector<float> cosTable; // cos(2*pi*k/n), k < n/2
    std::vector<float> sinTable; // sin(2*pi*k/n), k < n/2
};
}
