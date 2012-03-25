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


#ifndef OSKAR_GET_IMAGE_BASELINE_COORDS_H_
#define OSKAR_GET_IMAGE_BASELINE_COORDS_H_

/**
 * @file oskar_get_image_baseline_coords.h
 */

#include "oskar_global.h"

#include "utility/oskar_Mem.h"
#include "interferometry/oskar_Visibilities.h"
#include "imaging/oskar_SettingsImage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 * Get visibility coordinates required for imaging.
 *
 * @details
 * Populates uu and vv from visibilities structure for the particular
 * channel and time selection given the image settings of time and channel
 * range and snapshot imaging modes.
 *
 * WARNING: This function is intended to ONLY be used with oskar_make_image().
 * WARNING: This function relies on uu and vv being preallocated correctly.
 *
 * @param uu
 * @param vv
 * @param vis
 * @param time
 * @param settings
 * @return
 */
OSKAR_EXPORT
int oskar_get_image_baseline_coords(oskar_Mem* uu, oskar_Mem* vv,
        const oskar_Visibilities* vis, int time,
        const oskar_SettingsImage* settings);

#ifdef __cplusplus
}
#endif

#endif /* OSKAR_GET_IMAGE_BASELINE_COORDS_H_ */