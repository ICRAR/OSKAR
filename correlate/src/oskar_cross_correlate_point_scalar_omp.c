/*
 * Copyright (c) 2014-2015, The University of Oxford
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

#include <math.h>
#include <private_correlate_functions_inline.h>
#include <oskar_cross_correlate_point_scalar_omp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single precision. */
void oskar_cross_correlate_point_scalar_omp_f(int num_sources, int num_stations,
        const float2* jones, const float* source_I, const float* source_l,
        const float* source_m, const float* source_n, const float* station_u,
        const float* station_v, const float* station_w, float uv_min_lambda,
        float uv_max_lambda, float inv_wavelength, float frac_bandwidth,
        float2* vis)
{
    int SQ;

    /* Loop over stations. */
#pragma omp parallel for private(SQ) schedule(dynamic, 1)
    for (SQ = 0; SQ < num_stations; ++SQ)
    {
        int SP, i;
        const float2 *station_p, *station_q;

        /* Pointer to source vector for station q. */
        station_q = &jones[SQ * num_sources];

        /* Loop over baselines for this station. */
        for (SP = SQ + 1; SP < num_stations; ++SP)
        {
            float uv_len, uu, vv, ww, uu2, vv2, uuvv;
            float2 sum, guard;
            sum.x = 0.0f;
            sum.y = 0.0f;
            guard.x = 0.0f;
            guard.y = 0.0f;

            /* Pointer to source vector for station p. */
            station_p = &jones[SP * num_sources];

            /* Get common baseline values. */
            oskar_evaluate_baseline_terms_inline_f(station_u[SP],
                    station_u[SQ], station_v[SP], station_v[SQ],
                    station_w[SP], station_w[SQ], inv_wavelength,
                    frac_bandwidth, &uv_len, &uu, &vv, &ww, &uu2, &vv2, &uuvv);

            /* Apply the baseline length filter. */
            if (uv_len < uv_min_lambda || uv_len > uv_max_lambda)
                continue;

            /* Loop over sources. */
            for (i = 0; i < num_sources; ++i)
            {
                float l, m, n, rb;

                /* Get source direction cosines. */
                l = source_l[i];
                m = source_m[i];
                n = source_n[i];

                /* Compute bandwidth-smearing term. */
                rb = oskar_sinc_f(uu * l + vv * m + ww * (n - 1.0f));

                /* Accumulate baseline visibility response for source. */
                oskar_accumulate_baseline_visibility_for_source_scalar_inline_f(
                        &sum, i, source_I, station_p, station_q, rb, &guard);
            }

            /* Add result to the baseline visibility. */
            i = oskar_evaluate_baseline_index_inline(num_stations, SP, SQ);
            vis[i].x += sum.x;
            vis[i].y += sum.y;
        }
    }
}

/* Double precision. */
void oskar_cross_correlate_point_scalar_omp_d(int num_sources, int num_stations,
        const double2* jones, const double* source_I, const double* source_l,
        const double* source_m, const double* source_n, const double* station_u,
        const double* station_v, const double* station_w, double uv_min_lambda,
        double uv_max_lambda, double inv_wavelength, double frac_bandwidth,
        double2* vis)
{
    int SQ;

    /* Loop over stations. */
#pragma omp parallel for private(SQ) schedule(dynamic, 1)
    for (SQ = 0; SQ < num_stations; ++SQ)
    {
        int SP, i;
        const double2 *station_p, *station_q;

        /* Pointer to source vector for station q. */
        station_q = &jones[SQ * num_sources];

        /* Loop over baselines for this station. */
        for (SP = SQ + 1; SP < num_stations; ++SP)
        {
            double uv_len, uu, vv, ww, uu2, vv2, uuvv;
            double2 sum;
            sum.x = 0.0;
            sum.y = 0.0;

            /* Pointer to source vector for station p. */
            station_p = &jones[SP * num_sources];

            /* Get common baseline values. */
            oskar_evaluate_baseline_terms_inline_d(station_u[SP],
                    station_u[SQ], station_v[SP], station_v[SQ],
                    station_w[SP], station_w[SQ], inv_wavelength,
                    frac_bandwidth, &uv_len, &uu, &vv, &ww, &uu2, &vv2, &uuvv);

            /* Apply the baseline length filter. */
            if (uv_len < uv_min_lambda || uv_len > uv_max_lambda)
                continue;

            /* Loop over sources. */
            for (i = 0; i < num_sources; ++i)
            {
                double l, m, n, rb;

                /* Get source direction cosines. */
                l = source_l[i];
                m = source_m[i];
                n = source_n[i];

                /* Compute bandwidth-smearing term. */
                rb = oskar_sinc_d(uu * l + vv * m + ww * (n - 1.0));

                /* Accumulate baseline visibility response for source. */
                oskar_accumulate_baseline_visibility_for_source_scalar_inline_d(
                        &sum, i, source_I, station_p, station_q, rb);
            }

            /* Add result to the baseline visibility. */
            i = oskar_evaluate_baseline_index_inline(num_stations, SP, SQ);
            vis[i].x += sum.x;
            vis[i].y += sum.y;
        }
    }
}

#ifdef __cplusplus
}
#endif