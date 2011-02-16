/*
 * Copyright (c) 2011, The University of Oxford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Oxford nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "cuda/_imager2dDft.h"

// Shared memory pointer used by the kernel.
extern __shared__ float2 smem[];

#ifndef M_2PI
#define M_2PI 6.283185307f
#endif

/**
 * @details
 * This CUDA kernel computes a real image from a set of complex visibilities,
 * using a 2D Direct Fourier Transform (DFT).
 *
 * The computed image is returned in the \p image array, which
 * must be pre-sized to length \p np.
 *
 * Each thread evaluates a single pixel of the image, which is assumed to be
 * completely real: conjugated copies of the visibilities should therefore NOT
 * be supplied to this kernel.
 *
 * The kernel requires (4 * maxVisPerBlock + blockDim.x) * sizeof(float) bytes
 * of shared memory.
 *
 * @param[in] nv No. of independent visibilities (excluding Hermitian copy).
 * @param[in] u Array of visibility u coordinates in wavelengths (length nv).
 * @param[in] v Array of visibility v coordinates in wavelengths (length nv).
 * @param[in] vis Array of complex visibilities (length nv; see note, above).
 * @param[in] np The number of pixels in the image.
 * @param[in] pl The pixel l-positions on the orthographic tangent plane.
 * @param[in] pm The pixel m-positions on the orthographic tangent plane.
 * @param[in] maxVisPerBlock Maximum visibilities per block (multiple of 16).
 * @param[out] image The computed image (see note, above).
 */
__global__
void _imager2dDft(int nv, const float* u, const float* v,
        const float2* vis, const int np, const float* pl, const float* pm,
        const int maxVisPerBlock, float* image)
{
    // Get the pixel ID that this thread is working on.
    const int p = blockDim.x * blockIdx.x + threadIdx.x;

    // Get the pixel position.
    // (NB. Cannot exit on index condition, as all threads are needed later).
    float l = 0.0f, m = 0.0f;
    if (p < np) {
        l = pl[p];
        m = pm[p];
    }

    // Initialise shared memory caches.
    float2* cvs = smem; // Cached visibilities.
    float2* cuv = cvs + maxVisPerBlock; // Cached u,v-coordinates.
    float*  cpx = (float*)(cuv + maxVisPerBlock); // Cached pixel values.
    cpx[threadIdx.x] = 0.0f; // Clear pixel value.

    // Cache a block of visibilities and u,v-coordinates into shared memory.
    const int blocks = (nv + maxVisPerBlock - 1) / maxVisPerBlock;
    for (int block = 0; block < blocks; ++block) {
        const int visStart = block * maxVisPerBlock;
        int visInBlock = nv - visStart;
        if (visInBlock > maxVisPerBlock)
            visInBlock = maxVisPerBlock;

        // There are blockDim.x threads available - need to copy
        // visInBlock pieces of data from global memory.
        for (int t = threadIdx.x; t < visInBlock; t += blockDim.x) {
            const int vg = visStart + t; // Global visibility index.
            cvs[t] = vis[vg];
            cuv[t].x = u[vg];
            cuv[t].y = v[vg];
        }

        // Must synchronise before computing the signal for these visibilities.
        __syncthreads();

        // Loop over visibilities in block.
        for (int v = 0; v < visInBlock; ++v) {
            // Calculate the complex DFT weight.
            float2 weight;
            const float a = M_2PI * (cuv[v].x * l + cuv[v].y * m); // u*l + v*m
            __sincosf(a, &weight.y, &weight.x);

            // Perform complex multiply-accumulate.
            // Image is real, so should only need to evaluate the real part.
            cpx[threadIdx.x] += cvs[v].x * weight.x - cvs[v].y * weight.y;
        }

        // Must synchronise again before loading in a new block of visibilities.
        __syncthreads();
    }

    // Copy shared memory back into global memory.
    if (p < np)
        if (hypotf(l, m) > 1.0f)
            image[p] = 0.0f;
        else
            image[p] = cpx[threadIdx.x] / (float)nv;
}
