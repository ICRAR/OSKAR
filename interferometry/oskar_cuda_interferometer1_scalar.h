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

#ifndef OSKAR_INTERFEROMETRY_CUDA_INTERFEROMETER1_SCALAR_H_
#define OSKAR_INTERFEROMETRY_CUDA_INTERFEROMETER1_SCALAR_H_

/**
 * @file oskar_interferometry_cuda_interferometer1_scalar.h
 */

#include "oskar_windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 * @details
 *
 * @param[in] na   Number of antennas or stations.
 * @param[in] ax   Array of local equatorial station x-positions in wavenumbers.
 * @param[in] ay   Array of local equatorial station y-positions in wavenumbers.
 * @param[in] az   Array of local equatorial station z-positions in wavenumbers.
 * @param[in] ns   Number of sources.
 * @param[in] l    Array of source l-positions.
 * @param[in] m    Array of source m-positions.
 * @param[in] n    Array of source n-positions (see note, above).
 * @param[in] eb   Matrix of E * sqrt(B) (see note, above).
 * @param[in] ra0  Right Ascension of the phase tracking centre in radians.
 * @param[in] dec0 Declination of the phase tracking centre in radians.
 * @param[in] lst0 The local sidereal time at the start of the correlator dump.
 * @param[in] nsdt The number of averaging cycles to do.
 * @param[in] sdt  The time interval between averages in seconds.
 * @param[in] lambda_bandwidth Wavelength (m) times bandwidth (Hz).
 * @param[in,out]  vis The complex visibilities (see note, above).
 * @param[in,out]  work Work array of size 2 * ns * na + 3 * na.
 */
DllExport
int oskar_cudad_interferometer1_scalar(

        unsigned num_antennas,
        const float* antenna_x,
        const float* antenna_y,
        const float* antenna_z,

        unsigned num_sources,
        const float* source_l,
        const float* source_m,
        const float* source_n,
        const float* eb,

        float ra0,
        float dec0,
        float lst0,

        const unsigned nsdt,
        float sdt,
        float lambda_bandwidth,

        float* vis,
        float* work);


#ifdef __cplusplus
}
#endif

#endif // OSKAR_INTERFEROMETRY_CUDA_INTERFEROMETER1_SCALAR_H_
