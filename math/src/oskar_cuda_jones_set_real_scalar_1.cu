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

#include "math/oskar_cuda_jones_set_real_scalar_1.h"
#include "math/cudak/oskar_cudak_jones_set_real_scalar_1.h"

// Single precision.
int oskar_cuda_jones_set_real_scalar_1_f(int n, float2* d_jones,
        float scalar)
{
    // Set up the thread blocks.
    int n_thd = 256;
    int n_blk = (n + n_thd - 1) / n_thd;

    // Call the kernel.
    oskar_cudak_jones_set_real_scalar_1_f OSKAR_CUDAK_CONF(n_blk, n_thd) (n,
            d_jones, scalar);

    // Check for errors.
    cudaDeviceSynchronize();
    return cudaPeekAtLastError();
}

// Double precision.
int oskar_cuda_jones_set_real_scalar_1_d(int n, double2* d_jones,
        double scalar)
{
    // Set up the thread blocks.
    int n_thd = 256;
    int n_blk = (n + n_thd - 1) / n_thd;

    // Call the kernel.
    oskar_cudak_jones_set_real_scalar_1_d OSKAR_CUDAK_CONF(n_blk, n_thd) (n,
            d_jones, scalar);

    // Check for errors.
    cudaDeviceSynchronize();
    return cudaPeekAtLastError();
}