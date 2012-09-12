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

#include "test/Test_dierckx.h"
#include "math/oskar_SplineData.h"
#include "math/oskar_SettingsSpline.h"
#include "math/oskar_spline_data_copy.h"
#include "math/oskar_spline_data_init.h"
#include "math/oskar_spline_data_surfit.h"
#include "math/oskar_spline_data_evaluate.h"
#include "utility/oskar_mem_all_headers.h"
#include "utility/oskar_get_error_string.h"

#define TIMER_ENABLE 1
#include "utility/timer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using std::vector;

/**
 * @details
 * Converts the parameter to a C++ string.
 */
#include <sstream>

template <class T>
inline std::string oskar_to_std_string(const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

extern "C"
{
void bispev_(float tx[], int* nx, float ty[], int* ny, float c[],
        int* kx, int* ky, float x[], int* mx, float y[], int* my,
        float z[], float wrk[], int* lwrk, int iwrk[], int* kwrk, int* ier);

void surfit_(int* iopt, int* m, float* x, float* y, float* z, float* w,
        float* xb, float* xe, float* yb, float* ye, int* kx, int* ky, float* s,
        int* nxest, int* nyest, int* nmax, float* eps, int* nx, float* tx,
        int* ny, float* ty, float* c, float* fp, float* wrk1, int* lwrk1,
        float* wrk2, int* lwrk2, int* iwrk, int* kwrk, int* ier);
}

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

static int oskar_spline_data_surfit_fortran(oskar_SplineData* spline,
        int num_points, oskar_Mem* x, oskar_Mem* y, oskar_Mem* z,
        oskar_Mem* w, const oskar_SettingsSpline* settings)
{
    int element_size, err = 0, k = 0, maxiter = 1000, type;
    int b1, b2, bx, by, iopt, km, kwrk, lwrk1, lwrk2, ne, nxest, nyest, u, v;
    int sqrt_num_points;
    int *iwrk;
    void *wrk1, *wrk2;
    float x_beg, x_end, y_beg, y_end;

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
    x_beg = (float) oskar_mem_min(x, num_points);
    x_end = (float) oskar_mem_max(x, num_points);
    y_beg = (float) oskar_mem_min(y, num_points);
    y_end = (float) oskar_mem_max(y, num_points);

    /* Initialise and allocate spline data. */
    sqrt_num_points = (int)sqrt(num_points);
    nxest = kx + 1 + sqrt_num_points;
    nyest = ky + 1 + sqrt_num_points;
    u = nxest - kx - 1;
    v = nyest - ky - 1;
    err = oskar_spline_data_init(spline, type, OSKAR_LOCATION_CPU);
    if (err) return err;
    oskar_mem_realloc(&spline->knots_x, nxest, &err);
    oskar_mem_realloc(&spline->knots_y, nyest, &err);
    oskar_mem_realloc(&spline->coeff, u * v, &err);
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
                surfit_(&iopt, &num_points, (float*)x->data,
                        (float*)y->data, (float*)z->data,
                        (float*)w->data, &x_beg, &x_end,
                        &y_beg, &y_end, &kx, &ky, &s, &nxest, &nyest,
                        &ne, &eps, &spline->num_knots_x, knots_x,
                        &spline->num_knots_y, knots_y, coeff, &fp,
                        (float*)wrk1, &lwrk1, (float*)wrk2, &lwrk2, iwrk,
                        &kwrk, &err);
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
    else
        err = OSKAR_ERR_BAD_DATA_TYPE;

    /* Free work arrays. */
stop:
    free(iwrk);
    free(wrk2);
    free(wrk1);

    return err;
}

static int oskar_spline_data_evaluate_fortran(oskar_Mem* output, int offset,
        int stride, oskar_SplineData* spline, const oskar_Mem* x,
        const oskar_Mem* y)
{
    int err = 0, nx, ny, num_points, type, location;

    /* Check arrays are consistent. */
    num_points = x->num_elements;
    if (y->num_elements != num_points)
        return OSKAR_ERR_DIMENSION_MISMATCH;

    /* Check type. */
    type = x->type;
    if (type != y->type)
        return OSKAR_ERR_TYPE_MISMATCH;

    /* Check location. */
    location = output->location;
    if (location != spline->coeff.location ||
            location != spline->knots_x.location ||
            location != spline->knots_y.location ||
            location != x->location ||
            location != y->location)
        return OSKAR_ERR_BAD_LOCATION;

    /* Check data type. */
    if (type == OSKAR_SINGLE)
    {
        float *knots_x, *knots_y, *coeff;
        float *out;
        nx      = spline->num_knots_x;
        ny      = spline->num_knots_y;
        knots_x = (float*)spline->knots_x.data;
        knots_y = (float*)spline->knots_y.data;
        coeff   = (float*)spline->coeff.data;
        out     = (float*)output->data + offset;

        /* Check if data are in CPU memory. */
        if (location == OSKAR_LOCATION_CPU)
        {
            /* Set up workspace. */
            float wrk[8];
            int i, iwrk1[2], kwrk1 = 2, lwrk = 8;
            int kx = 3, ky = 3;
            int one = 1;

            /* Evaluate surface at the points. */
            for (i = 0; i < num_points; ++i)
            {
                float x1, y1;
                x1 = ((const float*)x->data)[i];
                y1 = ((const float*)y->data)[i];
                bispev_(knots_x, &nx, knots_y, &ny, coeff,
                        &kx, &ky, &x1, &one, &y1, &one, &out[i * stride],
                        wrk, &lwrk, iwrk1, &kwrk1, &err);
                if (err != 0) return OSKAR_ERR_SPLINE_EVAL_FAIL;
            }
        }
        else
            return OSKAR_ERR_BAD_LOCATION;
    }
    else
        return OSKAR_ERR_BAD_DATA_TYPE;

    return err;
}

void Test_dierckx::test_surfit()
{
    int err = 0;

    // Set data dimensions.
    int size_x_in = 20;
    int size_y_in = 10;
    int num_points_in = size_x_in * size_y_in;

    // Set up the input data.
    oskar_Mem x_in(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_in);
    oskar_Mem y_in(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_in);
    oskar_Mem z_in(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_in);
    oskar_Mem w(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_in);
    for (int y = 0, i = 0; y < size_y_in; ++y)
    {
        float y1 = y * (2.0 * M_PI) / (size_y_in - 1);
        for (int x = 0; x < size_x_in; ++x, ++i)
        {
            float x1 = x * (M_PI / 2.0) / (size_x_in - 1);

            // Store the data points.
            ((float*)x_in)[i] = x1;
            ((float*)y_in)[i] = y1;
            ((float*)z_in)[i] = cos(x1) * sin(y1); // Value of function at x,y.
            ((float*)w)[i]    = 1.0; // Weight.
        }
    }

    // Set up the surface fitting parameters.
    oskar_SettingsSpline settings;
    settings.average_fractional_error = 0.002;
    settings.average_fractional_error_factor_increase = 1.5;
    settings.eps_double = 2e-8;
    settings.eps_float = 4e-4;
    settings.search_for_best_fit = 1;
    settings.smoothness_factor_override = 1.0;
    settings.smoothness_factor_reduction = 0.9;

    // Set up the spline data (Fortran and C versions).
    oskar_SplineData spline_data_fortran;
    oskar_SplineData spline_data_c;
    err = oskar_spline_data_surfit_fortran(&spline_data_fortran, num_points_in,
            &x_in, &y_in, &z_in, &w, &settings);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);
    err = oskar_spline_data_surfit(&spline_data_c, NULL, num_points_in,
            &x_in, &y_in, &z_in, &w, &settings, "test");
    CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);

    // Check results are consistent.
    double delta = 1e-5;
    CPPUNIT_ASSERT_EQUAL(spline_data_fortran.num_knots_x,
            spline_data_c.num_knots_x);
    CPPUNIT_ASSERT_EQUAL(spline_data_fortran.num_knots_y,
            spline_data_c.num_knots_y);
    for (int i = 0; i < spline_data_c.num_knots_x; ++i)
        CPPUNIT_ASSERT_DOUBLES_EQUAL(((float*)spline_data_fortran.knots_x)[i],
                ((float*)spline_data_c.knots_x)[i], delta);
    for (int i = 0; i < spline_data_c.num_knots_y; ++i)
        CPPUNIT_ASSERT_DOUBLES_EQUAL(((float*)spline_data_fortran.knots_y)[i],
                ((float*)spline_data_c.knots_y)[i], delta);
    for (int i = 0; i < spline_data_c.coeff.num_elements; ++i)
        CPPUNIT_ASSERT_DOUBLES_EQUAL(((float*)spline_data_fortran.coeff)[i],
                ((float*)spline_data_c.coeff)[i], delta);

    // Evaluate output point positions.
    int size_x_out = 100;
    int size_y_out = 200;
    int num_points_out = size_x_out * size_y_out;
    oskar_Mem x_out(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_out);
    oskar_Mem y_out(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_out);
    for (int y = 0, i = 0; y < size_y_out; ++y)
    {
        float y1 = y * (2.0 * M_PI) / (size_y_out - 1);
        for (int x = 0; x < size_x_out; ++x, ++i)
        {
            float x1 = x * (M_PI / 2.0) / (size_x_out - 1);
            ((float*)x_out)[i] = x1;
            ((float*)y_out)[i] = y1;
        }
    }

    // Evaluate surface (Fortran).
    oskar_Mem z_out_fortran(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_out);
    TIMER_START
    err = oskar_spline_data_evaluate_fortran(&z_out_fortran, 0, 1,
            &spline_data_fortran, &x_out, &y_out);
    TIMER_STOP("Finished surface evaluation [Fortran] (%d points)",
            num_points_out);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);

    // Evaluate surface (C).
    oskar_Mem z_out_c(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_out);
    TIMER_START
    err = oskar_spline_data_evaluate(&z_out_c, 0, 1, &spline_data_c,
            &x_out, &y_out);
    TIMER_STOP("Finished surface evaluation [C] (%d points)", num_points_out);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);

    // Evaluate surface (CUDA).
    oskar_Mem z_out_cuda(OSKAR_SINGLE, OSKAR_LOCATION_CPU, num_points_out);
    {
        // Copy the spline data to the GPU.
        oskar_SplineData spline_data_cuda;
        err = oskar_spline_data_init(&spline_data_cuda, OSKAR_SINGLE,
                OSKAR_LOCATION_GPU);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);
        oskar_spline_data_copy(&spline_data_cuda, &spline_data_c, &err);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);

        // Copy the x,y positions to the GPU and allocate memory for result.
        oskar_Mem x_out_temp(&x_out, OSKAR_LOCATION_GPU);
        oskar_Mem y_out_temp(&y_out, OSKAR_LOCATION_GPU);
        oskar_Mem z_out_temp(OSKAR_SINGLE, OSKAR_LOCATION_GPU, num_points_out);

        // Do the evaluation.
        TIMER_START
        err = oskar_spline_data_evaluate(&z_out_temp, 0, 1, &spline_data_cuda,
                &x_out_temp, &y_out_temp);
        TIMER_STOP("Finished surface evaluation [CUDA] (%d points)",
                num_points_out);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);

        // Copy the memory back.
        oskar_mem_copy(&z_out_cuda, &z_out_temp, &err);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(oskar_get_error_string(err), 0, err);
    }

    // Check results are consistent.
    for (int i = 0; i < num_points_out; ++i)
    {
        CPPUNIT_ASSERT_DOUBLES_EQUAL(((float*)z_out_fortran)[i],
                ((float*)z_out_c)[i], 1e-6);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(((float*)z_out_c)[i],
                ((float*)z_out_cuda)[i], 1e-6);
    }

    // Write out the data.
    FILE* file = fopen("test_surfit.dat", "w");
    for (int i = 0; i < num_points_out; ++i)
    {
        fprintf(file, "%10.6f %10.6f %10.6f\n ",
                ((float*)x_out)[i], ((float*)y_out)[i], ((float*)z_out_c)[i]);
    }
    fclose(file);
}
