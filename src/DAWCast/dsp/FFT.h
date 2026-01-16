// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace dawcast {

/// Header-only radix-2 Cooley-Tukey FFT for real-time DSP.
/// Size must be a power of 2.

class FFT
{
public:
    explicit FFT(int size)
        : m_size(size)
        , m_cosTable(size / 2)
        , m_sinTable(size / 2)
    {
        // Pre-compute twiddle factors
        for (int i = 0; i < size / 2; ++i) {
            double angle = -2.0 * M_PI * i / size;
            m_cosTable[i] = static_cast<float>(std::cos(angle));
            m_sinTable[i] = static_cast<float>(std::sin(angle));
        }
    }

    int size() const { return m_size; }

    /// Real-to-complex forward FFT.
    /// input: m_size real samples
    /// real, imag: m_size floats each (output complex spectrum)
    void forward(const float* input, float* real, float* imag)
    {
        // Copy input into real, zero imag
        for (int i = 0; i < m_size; ++i) {
            real[i] = input[i];
            imag[i] = 0.0f;
        }

        // Bit-reversal permutation
        bitReverse(real, imag);

        // Cooley-Tukey butterfly
        for (int len = 2; len <= m_size; len <<= 1) {
            int halfLen = len / 2;
            int tableStep = m_size / len;

            for (int i = 0; i < m_size; i += len) {
                for (int j = 0; j < halfLen; ++j) {
                    int tIdx = j * tableStep;
                    float tRe = m_cosTable[tIdx] * real[i + j + halfLen]
                              - m_sinTable[tIdx] * imag[i + j + halfLen];
                    float tIm = m_cosTable[tIdx] * imag[i + j + halfLen]
                              + m_sinTable[tIdx] * real[i + j + halfLen];

                    real[i + j + halfLen] = real[i + j] - tRe;
                    imag[i + j + halfLen] = imag[i + j] - tIm;
                    real[i + j] += tRe;
                    imag[i + j] += tIm;
                }
            }
        }
    }

    /// Complex-to-real inverse FFT.
    /// real, imag: m_size floats each (input complex spectrum)
    /// output: m_size real samples (normalized by 1/N)
    void inverse(const float* real, const float* imag, float* output)
    {
        // Copy and conjugate (negate imag for IFFT via forward FFT)
        std::vector<float> re(m_size), im(m_size);
        for (int i = 0; i < m_size; ++i) {
            re[i] = real[i];
            im[i] = -imag[i];
        }

        // Bit-reversal permutation
        bitReverse(re.data(), im.data());

        // Same butterfly as forward
        for (int len = 2; len <= m_size; len <<= 1) {
            int halfLen = len / 2;
            int tableStep = m_size / len;

            for (int i = 0; i < m_size; i += len) {
                for (int j = 0; j < halfLen; ++j) {
                    int tIdx = j * tableStep;
                    float tRe = m_cosTable[tIdx] * re[i + j + halfLen]
                              - m_sinTable[tIdx] * im[i + j + halfLen];
                    float tIm = m_cosTable[tIdx] * im[i + j + halfLen]
                              + m_sinTable[tIdx] * re[i + j + halfLen];

                    re[i + j + halfLen] = re[i + j] - tRe;
                    im[i + j + halfLen] = im[i + j] - tIm;
                    re[i + j] += tRe;
                    im[i + j] += tIm;
                }
            }
        }

        // Normalize and output real part
        float invN = 1.0f / static_cast<float>(m_size);
        for (int i = 0; i < m_size; ++i) {
            output[i] = re[i] * invN;
        }
    }

private:
    void bitReverse(float* re, float* im)
    {
        int n = m_size;
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            while (j & bit) {
                j ^= bit;
                bit >>= 1;
            }
            j ^= bit;

            if (i < j) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
    }

    int m_size;
    std::vector<float> m_cosTable;
    std::vector<float> m_sinTable;
};

} // namespace dawcast
