/*
 * Copyright (c) 2016, The University of Oxford
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

#include <private_imager.h>

#include <oskar_binary.h>
#include <oskar_imager_set_options.h>
#include <oskar_imager_run.h>
#include <oskar_imager_finalise.h>
#include <oskar_imager_update.h>
#include <oskar_vis_block.h>
#include <oskar_vis_header.h>

#include <ms/oskar_measurement_set.h>

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static void oskar_imager_run_ms(oskar_Imager* h, const char* filename,
        int* status);
static void oskar_imager_run_vis(oskar_Imager* h, const char* filename,
        int* status);

void oskar_imager_run(oskar_Imager* h, const char* filename, int* status)
{
    int len, use_ms;
    if (*status || !filename) return;

    /* Check filename for Measurement Set. */
    len = strlen(filename);
    if (len == 0) { *status = OSKAR_ERR_FILE_IO; return; }
    use_ms = (len >= 3) && (
            !strcmp(&filename[len-3], ".MS") ||
            !strcmp(&filename[len-3], ".ms") ) ? 1 : 0;
    if (use_ms)
        oskar_imager_run_ms(h, filename, status);
    else
        oskar_imager_run_vis(h, filename, status);

    /* Finalise the image plane(s) and write them out. */
    oskar_imager_finalise(h, 0, status);
}

void oskar_imager_run_vis(oskar_Imager* h, const char* filename, int* status)
{
    oskar_Binary* vis_file;
    oskar_VisBlock* blk;
    oskar_VisHeader* hdr;
    oskar_Mem* weight;
    int max_times_per_block, tags_per_block, i_block, num_blocks;
    int start_time, end_time, start_chan, end_chan;
    int num_times, num_channels, num_stations, num_baselines, num_pols;
    int percent_done = 0, percent_next = 10;
    int dim_start_and_size[6];
    if (h->log) oskar_log_message(h->log, 'M', 0,
            "Opening OSKAR visibility file '%s'", filename);
    vis_file = oskar_binary_create(filename, 'r', status);
    hdr = oskar_vis_header_read(vis_file, status);
    if (*status)
    {
        oskar_vis_header_free(hdr, status);
        oskar_binary_free(vis_file);
        return;
    }
    max_times_per_block = oskar_vis_header_max_times_per_block(hdr);
    tags_per_block = oskar_vis_header_num_tags_per_block(hdr);
    num_times = oskar_vis_header_num_times_total(hdr);
    num_channels = oskar_vis_header_num_channels_total(hdr);
    num_stations = oskar_vis_header_num_stations(hdr);
    num_baselines = num_stations * (num_stations - 1) / 2;
    num_pols = oskar_type_is_matrix(oskar_vis_header_amp_type(hdr)) ? 4 : 1;

    /* Set visibility meta-data. */
    oskar_imager_set_vis_frequency(h,
            oskar_vis_header_freq_start_hz(hdr),
            oskar_vis_header_freq_inc_hz(hdr), num_channels, status);
    oskar_imager_set_vis_time(h,
            oskar_vis_header_time_start_mjd_utc(hdr),
            oskar_vis_header_time_inc_sec(hdr), num_times, status);
    oskar_imager_set_vis_phase_centre(h,
            oskar_vis_header_phase_centre_ra_deg(hdr),
            oskar_vis_header_phase_centre_dec_deg(hdr));
    if (*status)
    {
        oskar_vis_header_free(hdr, status);
        oskar_binary_free(vis_file);
        return;
    }

    /* Create weights array and set all to 1. */
    weight = oskar_mem_create(
            oskar_type_precision(oskar_vis_header_amp_type(hdr)),
            OSKAR_CPU, num_baselines * num_pols * max_times_per_block, status);
    oskar_mem_set_value_real(weight, 1.0, 0, 0, status);

    /* Loop over visibility blocks. */
    blk = oskar_vis_block_create(OSKAR_CPU, hdr, status);
    num_blocks = (num_times + max_times_per_block - 1) /
            max_times_per_block;
    if (h->log)
    {
        oskar_log_message(h->log, 'S', -2, "");
        oskar_log_message(h->log, 'S', -2, "%3d%% ...", 0);
    }
    for (i_block = 0; i_block < num_blocks; ++i_block)
    {
        if (*status) break;

        /* Read block metadata. */
        oskar_binary_set_query_search_start(vis_file,
                i_block * tags_per_block, status);
        oskar_binary_read(vis_file, OSKAR_INT,
                OSKAR_TAG_GROUP_VIS_BLOCK,
                OSKAR_VIS_BLOCK_TAG_DIM_START_AND_SIZE, i_block,
                sizeof(dim_start_and_size), dim_start_and_size, status);
        start_time = dim_start_and_size[0];
        start_chan = dim_start_and_size[1];
        end_time   = start_time + dim_start_and_size[2] - 1;
        end_chan   = start_chan + dim_start_and_size[3] - 1;

        /* Check that at least part of the block is in range. */
        if (end_time >= h->time_range[0] &&
                (start_time <= h->time_range[1] || h->time_range[1] < 0))
        {
            oskar_vis_block_read(blk, hdr, vis_file, i_block, status);
            oskar_imager_update(h, start_time, end_time, start_chan, end_chan,
                    oskar_vis_block_num_pols(blk),
                    oskar_vis_block_num_baselines(blk),
                    oskar_vis_block_baseline_uu_metres(blk),
                    oskar_vis_block_baseline_vv_metres(blk),
                    oskar_vis_block_baseline_ww_metres(blk),
                    oskar_vis_block_cross_correlations(blk), weight, status);
        }

        /* Update progress. */
        percent_done = 100 * (i_block + 1) / (double)num_blocks;
        if (percent_done >= percent_next)
        {
            if (h->log) oskar_log_message(h->log, 'S', -2, "%3d%% ...",
                    percent_done);
            percent_next += 10;
        }
    }
    if (h->log) oskar_log_message(h->log, 'S', -2, "");
    oskar_mem_free(weight, status);
    oskar_vis_block_free(blk, status);
    oskar_vis_header_free(hdr, status);
    oskar_binary_free(vis_file);
}


void oskar_imager_run_ms(oskar_Imager* h, const char* filename, int* status)
{
#ifndef OSKAR_NO_MS
    oskar_MeasurementSet* ms;
    oskar_Mem *uvw, *u, *v, *w, *data, *weight;
    int num_rows, num_stations, num_baselines, num_pols;
    int start_time, end_time, start_chan, end_chan;
    int num_times, num_channels, percent_done = 0, percent_next = 10;
    int i, block_size, start_row, type;
    double *uvw_, *u_, *v_, *w_;
    if (h->log) oskar_log_message(h->log, 'M', 0,
            "Opening Measurement Set '%s'", filename);
    ms = oskar_ms_open(filename);
    if (!ms)
    {
        *status = OSKAR_ERR_FILE_IO;
        return;
    }
    num_rows = (int) oskar_ms_num_rows(ms);
    num_stations = (int) oskar_ms_num_stations(ms);
    num_baselines = num_stations * (num_stations - 1) / 2;
    num_pols = (int) oskar_ms_num_pols(ms);
    num_channels = (int) oskar_ms_num_channels(ms);
    num_times = num_rows / num_baselines;
    start_time = end_time = 0;
    start_chan = 0; end_chan = num_channels - 1;

    /* Check for irregular data and override synthesis mode if required. */
    if (num_rows % num_baselines != 0)
    {
        oskar_log_warning(h->log,
                "Irregular data detected. Using full time synthesis.");
        oskar_imager_set_time_range(h, 0, -1, 0);
    }

    /* Set visibility meta-data. */
    oskar_imager_set_vis_frequency(h,
            oskar_ms_ref_freq_hz(ms),
            oskar_ms_channel_width_hz(ms), num_channels, status);
    oskar_imager_set_vis_time(h,
            oskar_ms_start_time_mjd(ms),
            oskar_ms_time_inc_sec(ms), num_times, status);
    oskar_imager_set_vis_phase_centre(h,
            oskar_ms_phase_centre_ra_rad(ms) * 180/M_PI,
            oskar_ms_phase_centre_dec_rad(ms) * 180/M_PI);
    if (*status)
    {
        oskar_ms_close(ms);
        return;
    }

    /* Create arrays. */
    uvw = oskar_mem_create(OSKAR_DOUBLE, OSKAR_CPU,
            3 * num_baselines, status);
    u = oskar_mem_create(OSKAR_DOUBLE, OSKAR_CPU, num_baselines, status);
    v = oskar_mem_create(OSKAR_DOUBLE, OSKAR_CPU, num_baselines, status);
    w = oskar_mem_create(OSKAR_DOUBLE, OSKAR_CPU, num_baselines, status);
    weight = oskar_mem_create(OSKAR_SINGLE, OSKAR_CPU,
            num_baselines * num_pols, status);
    uvw_ = oskar_mem_double(uvw, status);
    u_ = oskar_mem_double(u, status);
    v_ = oskar_mem_double(v, status);
    w_ = oskar_mem_double(w, status);
    type = OSKAR_SINGLE | OSKAR_COMPLEX;
    if (num_pols == 4) type |= OSKAR_MATRIX;
    data = oskar_mem_create(type, OSKAR_CPU,
            num_baselines * num_channels, status);

    /* Loop over visibility blocks. */
    if (h->log)
    {
        oskar_log_message(h->log, 'S', -2, "");
        oskar_log_message(h->log, 'S', -2, "%3d%% ...", 0);
    }
    for (start_row = 0; start_row < num_rows; start_row += num_baselines)
    {
        size_t allocated, required;
        if (*status) break;

        /* Read rows from Measurement Set. */
        block_size = num_rows - start_row;
        if (block_size > num_baselines) block_size = num_baselines;
        allocated = oskar_mem_length(uvw) *
                oskar_mem_element_size(oskar_mem_type(uvw));
        oskar_ms_get_column(ms, "UVW", start_row, block_size,
                allocated, oskar_mem_void(uvw), &required, status);
        allocated = oskar_mem_length(weight) *
                oskar_mem_element_size(oskar_mem_type(weight));
        oskar_ms_get_column(ms, "WEIGHT", start_row, block_size,
                allocated, oskar_mem_void(weight), &required, status);
        allocated = oskar_mem_length(data) *
                oskar_mem_element_size(oskar_mem_type(data));
        oskar_ms_get_column(ms, h->ms_column, start_row, block_size,
                allocated, oskar_mem_void(data), &required, status);
        if (*status) break;

        /* TODO(FD) Swap baseline and channel dimensions. */
        if (num_channels != 1)
        {
            oskar_log_error(h->log,
                    "oskar_imager currently works with only one channel.");
            *status = OSKAR_ERR_FUNCTION_NOT_AVAILABLE;
            break;
        }

        /* Split up baseline coordinates. */
        for (i = 0; i < block_size; ++i)
        {
            u_[i] = uvw_[3*i + 0];
            v_[i] = uvw_[3*i + 1];
            w_[i] = uvw_[3*i + 2];
        }

        /* Add the baseline data. */
        oskar_imager_update(h, start_time, end_time, start_chan, end_chan,
                num_pols, num_baselines, u, v, w, data, weight, status);
        start_time += 1;
        end_time += 1;

        /* Update progress. */
        percent_done = 100 * (start_row + num_baselines) / (double)num_rows;
        if (percent_done >= percent_next)
        {
            if (h->log) oskar_log_message(h->log, 'S', -2, "%3d%% ...",
                    percent_done);
            percent_next += 10;
        }
    }
    if (h->log) oskar_log_message(h->log, 'S', -2, "");
    oskar_mem_free(uvw, status);
    oskar_mem_free(u, status);
    oskar_mem_free(v, status);
    oskar_mem_free(w, status);
    oskar_mem_free(data, status);
    oskar_mem_free(weight, status);
    oskar_ms_close(ms);
#else
    oskar_log_error(log, "OSKAR was compiled without Measurement Set support.");
#endif
}


#ifdef __cplusplus
}
#endif