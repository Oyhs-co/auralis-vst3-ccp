// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace auralis
{
struct ProcessContext
{
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
};

class AudioBufferView
{
public:
    AudioBufferView (float* const* channels, int numChannels, int numFrames)
        : chans (channels), nch (numChannels), nf (numFrames) {}
    int numChannels() const { return nch; }
    int numFrames() const { return nf; }
    float* channel (int c) const { return chans[c]; }
private:
    float* const* chans;
    int nch;
    int nf;
};
}
