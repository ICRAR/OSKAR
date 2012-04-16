/*
 * Copyright (c) 2012, The University of Oxford
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

#include "math/oskar_dierckx_surfit.h"
#include "math/oskar_spline_data_surfit.h"
#include "math/oskar_spline_data_init.h"
#include "utility/oskar_Mem.h"
#include "utility/oskar_mem_element_size.h"
#include "utility/oskar_mem_init.h"
#include "utility/oskar_mem_realloc.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the largest absolute (real) value in the array. */
static double oskar_mem_max_abs(const oskar_Mem* data, int n)
{
    int i;
    double r = -DBL_MAX;
    if (data->type == OSKAR_SINGLE)
    {
        const float *p;
        p = (const float*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (fabsf(p[i]) > r) r = fabsf(p[i]);
        }
    }
    else if (data->type == OSKAR_DOUBLE)
    {
        const double *p;
        p = (const double*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (fabs(p[i]) > r) r = fabs(p[i]);
        }
    }
    return r;
}

/* Returns the largest value in the array. */
static double oskar_mem_max(const oskar_Mem* data, int n)
{
    int i;
    double r = -DBL_MAX;
    if (data->type == OSKAR_SINGLE)
    {
        const float *p;
        p = (const float*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (p[i] > r) r = p[i];
        }
    }
    else if (data->type == OSKAR_DOUBLE)
    {
        const double *p;
        p = (const double*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (p[i] > r) r = p[i];
        }
    }
    return r;
}

/* Returns the smallest value in the array. */
static double oskar_mem_min(const oskar_Mem* data, int n)
{
    int i;
    double r = DBL_MAX;
    if (data->type == OSKAR_SINGLE)
    {
        const float *p;
        p = (const float*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (p[i] < r) r = p[i];
        }
    }
    else if (data->type == OSKAR_DOUBLE)
    {
        const double *p;
        p = (const double*)data->data;
        for (i = 0; i < n; ++i)
        {
            if (p[i] < r) r = p[i];
        }
    }
    return r;
}

int oskar_spline_data_surfit(oskar_SplineData* spline, int num_points,
        oskar_Mem* x, oskar_Mem* y, const oskar_Mem* z, const oskar_Mem* w,
        const oskar_SettingsSpline* settings)
{
    int element_size, err, k = 0, maxiter = 1000, type;
    int b1, b2, bx, by, iopt, km, kwrk, lwrk1, lwrk2, ne, nxest, nyest, u, v;
    int sqrt_num_points;
    int *iwrk;
    void *wrk1, *wrk2;
    double x_beg, x_end, y_beg, y_end;

    /* Order of splines - do not change these values. */
    int kx = 3, ky = 3;

    /* Check that parameters are within allowed ranges. */
    if (settings->smoothness_factor_reduction >= 1.0 ||
            settings->smoothness_factor_reduction <= 0.0)
        return OSKAR_ERR_SETTINGS;
    if (settings->average_fractional_error_factor_increase <= 1.0)
        return OSKAR_ERR_SETTINGS;

    /* Get the data type. */
    type = z->type;
    element_size = oskar_mem_element_size(type);
    if ((type != OSKAR_SINGLE) && (type != OSKAR_DOUBLE))
        return OSKAR_ERR_BAD_DATA_TYPE;

    /* Check that input data is on the CPU. */
    if (x->location != OSKAR_LOCATION_CPU ||
            y->location != OSKAR_LOCATION_CPU ||
            z->location != OSKAR_LOCATION_CPU ||
            w->location != OSKAR_LOCATION_CPU)
        return OSKAR_ERR_BAD_LOCATION;

    /* Get data boundaries. */
    x_beg = oskar_mem_min(x, num_points);
    x_end = oskar_mem_max(x, num_points);
    y_beg = oskar_mem_min(y, num_points);
    y_end = oskar_mem_max(y, num_points);

    /* Initialise and allocate spline data. */
    sqrt_num_points = (int)sqrt(num_points);
    nxest = kx + 1 + sqrt_num_points;
    nyest = ky + 1 + sqrt_num_points;
    u = nxest - kx - 1;
    v = nyest - ky - 1;
    err = oskar_spline_data_init(spline, type, OSKAR_LOCATION_CPU);
    if (err) return err;
    err = oskar_mem_realloc(&spline->knots_x, nxest);
    if (err) return err;
    err = oskar_mem_realloc(&spline->knots_y, nyest);
    if (err) return err;
    err = oskar_mem_realloc(&spline->coeff, u * v);
    if (err) return err;

    /* Set up workspace. */
    km = 1 + ((kx > ky) ? kx : ky);
    ne = (nxest > nyest) ? nxest : nyest;
    bx = kx * v + ky + 1;
    by = ky * u + kx + 1;
    if (bx <= by)
    {
        b1 = bx;
        b2 = b1 + v - ky;
    }
    else
    {
        b1 = by;
        b2 = b1 + u - kx;
    }
    lwrk1 = u * v * (2 + b1 + b2) +
            2 * (u + v + km * (num_points + ne) + ne - kx - ky) + b2 + 1;
    lwrk2 = u * v * (b2 + 1) + b2;
    kwrk = num_points + (nxest - 2 * kx - 1) * (nyest - 2 * ky - 1);
    wrk1 = malloc(lwrk1 * element_size);
    wrk2 = malloc(lwrk2 * element_size);
    iwrk = (int*)malloc(kwrk * sizeof(int));
    if (wrk1 == NULL || wrk2 == NULL || iwrk == NULL)
        return OSKAR_ERR_MEMORY_ALLOC_FAILURE;

    if (type == OSKAR_SINGLE)
    {
        /* Set up the surface fitting parameters. */
        float eps, s, user_s, fp = 0.0;
        float *knots_x, *knots_y, *coeff, peak_abs, avg_frac_err_loc;
        int done = 0;
        eps              = (float)settings->eps_float;
        avg_frac_err_loc = (float)settings->average_fractional_error;
        knots_x          = (float*)spline->knots_x.data;
        knots_y          = (float*)spline->knots_y.data;
        coeff            = (float*)spline->coeff.data;
        peak_abs         = oskar_mem_max_abs(z, num_points);
        user_s           = (float)settings->smoothness_factor_override;
        do
        {
            float avg_err, term;
            avg_err = avg_frac_err_loc * peak_abs;
            term = num_points * avg_err * avg_err; /* Termination condition. */
            s = settings->search_for_best_fit ? 2.0 * term : user_s;
            for (k = 0, iopt = 0; k < maxiter; ++k)
            {
                if (k > 0) iopt = 1; /* Set iopt to 1 if not first pass. */
                oskar_dierckx_surfit_f(iopt, num_points, (float*)x->data,
                        (float*)y->data, (const float*)z->data,
                        (const float*)w->data, (float)x_beg, (float)x_end,
                        (float)y_beg, (float)y_end, kx, ky, s, nxest, nyest,
                        ne, eps, &spline->num_knots_x, knots_x,
                        &spline->num_knots_y, knots_y, coeff, &fp,
                        (float*)wrk1, lwrk1, (float*)wrk2, lwrk2, iwrk, kwrk,
                        &err);
                printf("Iteration %d, s = %.4e, fp = %.4e\n", k, s, fp);

                /* Check for errors. */
                if (err > 0 || err < -2) break;
                else if (err == -2) s = fp;

                /* Check if the fit is good enough. */
                if (!settings->search_for_best_fit || fp < term || s < term)
                    break;

                /* Decrease smoothing factor. */
                s *= settings->smoothness_factor_reduction;
            }

            /* Check for errors. */
            if (err > 0 || err < -2)
            {
                printf("Error (%d) finding spline coefficients.\n", err);
                if (!settings->search_for_best_fit || err == 10)
                {
                    err = OSKAR_ERR_SPLINE_COEFF_FAIL;
                    goto stop;
                }
                avg_frac_err_loc *= settings->average_fractional_error_factor_increase;
                printf("Increasing allowed average fractional error to %.3f.\n",
                        avg_frac_err_loc);
            }
            else
            {
                done = 1;
                err = 0;
                if (err == 5)
                {
                    printf("Cannot add any more knots.\n");
                    avg_frac_err_loc = sqrt(fp / num_points) / peak_abs;
                }
                if (settings->search_for_best_fit)
                {
                    printf("Surface fit to %.3f avg. frac. error "
                            "(s=%.2e, fp=%.2e, k=%d).\n", avg_frac_err_loc,
                            s, fp, k);
                }
                else
                {
                    printf("Surface fit (s=%.2e, fp=%.2e).\n", s, fp);
                }
                printf("Number of knots (x: %d, y: %d)\n",
                        spline->num_knots_x, spline->num_knots_y);
            }
        } while (settings->search_for_best_fit && !done);
    }
    else if (type == OSKAR_DOUBLE)
    {
        /* Set up the surface fitting parameters. */
        double eps, s, user_s, fp = 0.0;
        double *knots_x, *knots_y, *coeff, peak_abs, avg_frac_err_loc;
        int done = 0;
        eps              = (double)settings->eps_double;
        avg_frac_err_loc = (double)settings->average_fractional_error;
        knots_x          = (double*)spline->knots_x.data;
        knots_y          = (double*)spline->knots_y.data;
        coeff            = (double*)spline->coeff.data;
        peak_abs         = oskar_mem_max_abs(z, num_points);
        user_s           = (double)settings->smoothness_factor_override;
        do
        {
            double avg_err, term;
            avg_err = avg_frac_err_loc * peak_abs;
            term = num_points * avg_err * avg_err; /* Termination condition. */
            s = settings->search_for_best_fit ? 2.0 * term : user_s;
            for (k = 0, iopt = 0; k < maxiter; ++k)
            {
                if (k > 0) iopt = 1; /* Set iopt to 1 if not first pass. */
                oskar_dierckx_surfit_d(iopt, num_points, (double*)x->data,
                        (double*)y->data, (const double*)z->data,
                        (const double*)w->data, x_beg, x_end, y_beg, y_end,
                        kx, ky, s, nxest, nyest, ne, eps, &spline->num_knots_x,
                        knots_x, &spline->num_knots_y, knots_y, coeff, &fp,
                        (double*)wrk1, lwrk1, (double*)wrk2, lwrk2, iwrk, kwrk,
                        &err);
                printf("Iteration %d, s = %.4e, fp = %.4e\n", k, s, fp);

                /* Check for errors. */
                if (err > 0 || err < -2) break;
                else if (err == -2) s = fp;

                /* Check if the fit is good enough. */
                if (!settings->search_for_best_fit || fp < term || s < term)
                    break;

                /* Decrease smoothing factor. */
                s *= settings->smoothness_factor_reduction;
            }

            /* Check for errors. */
            if (err > 0 || err < -2)
            {
                printf("Error (%d) finding spline coefficients.\n", err);
                if (!settings->search_for_best_fit || err == 10)
                {
                    err = OSKAR_ERR_SPLINE_COEFF_FAIL;
                    goto stop;
                }
                avg_frac_err_loc *= settings->average_fractional_error_factor_increase;
                printf("Increasing allowed average fractional error to %.3f.\n",
                        avg_frac_err_loc);
            }
            else
            {
                done = 1;
                err = 0;
                if (err == 5)
                {
                    printf("Cannot add any more knots.\n");
                    avg_frac_err_loc = sqrt(fp / num_points) / peak_abs;
                }
                if (settings->search_for_best_fit)
                {
                    printf("Surface fit to %.3f avg. frac. error "
                            "(s=%.2e, fp=%.2e, k=%d).\n", avg_frac_err_loc,
                            s, fp, k);
                }
                else
                {
                    printf("Surface fit (s=%.2e, fp=%.2e).\n", s, fp);
                }
                printf("Number of knots (x: %d, y: %d)\n",
                        spline->num_knots_x, spline->num_knots_y);
            }
        } while (settings->search_for_best_fit && !done);
    }

    /* Free work arrays. */
stop:
    free(iwrk);
    free(wrk2);
    free(wrk1);

    return err;
}

#ifdef __cplusplus
}
#endif