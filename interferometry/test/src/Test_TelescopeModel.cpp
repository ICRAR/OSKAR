/*
 * Copyright (c) 2011-2013, The University of Oxford
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

#include <gtest/gtest.h>

#include <oskar_horizon_plane_to_offset_geocentric_cartesian.h>
#include <oskar_telescope_model_init.h>
#include <oskar_telescope_model_init_copy.h>
#include <oskar_telescope_model_free.h>
#include <oskar_telescope_model_load_station_coords.h>
#include <oskar_station_model_load_config.h>
#include <oskar_get_error_string.h>

#include <cmath>
#include <cstdio>

static const char* telescope_file_name = "test_telescope.dat";
static const char* station_base = "test_station";
static const int n_stations = 25;
static const int n_elements = 200;

static void create_test_data()
{
    // Create a telescope coordinate file.
    FILE* file;
    char station_name[80];
    file = fopen(telescope_file_name, "w");
    for (int i = 0; i < n_stations; ++i)
        fprintf(file, "%.8f,%.8f,%.8f\n", i / 10.0, i / 20.0, i / 30.0);
    fclose(file);

    // Create some station coordinate files.
    for (int i = 0; i < n_stations; ++i)
    {
        sprintf(station_name, "%s_%d.dat", station_base, i);
        file = fopen(station_name, "w");
        for (int j = 0; j < n_elements; ++j)
        {
            int t = j + i;
            fprintf(file, "%.8f,%.8f,%.8f\n", t / 5.0, t / 6.0, t / 7.0);
        }
        fclose(file);
    }
}

static void delete_test_data()
{
    char station_name[80];

    // Remove telescope coordinate file.
    remove(telescope_file_name);

    // Remove station coordinate files.
    for (int i = 0; i < n_stations; ++i)
    {
        sprintf(station_name, "%s_%d.dat", station_base, i);
        remove(station_name);
    }
}


TEST(TelescopeModel, load_telescope_cpu)
{
    create_test_data();

    char station_name[80];
    int status = 0;

    // Set the location.
    double longitude = 30.0 * M_PI / 180.0;
    double latitude = 50.0 * M_PI / 180.0;
    double altitude = 0.0;
    oskar_TelescopeModel tel_cpu, tel_cpu2, tel_gpu;
    oskar_telescope_model_init(&tel_cpu, OSKAR_DOUBLE,
            OSKAR_LOCATION_CPU, 0, &status);
    ASSERT_EQ(0, status) << oskar_get_error_string(status);

    // Fill the telescope structure.
    oskar_telescope_model_load_station_coords(&tel_cpu, telescope_file_name,
            longitude, latitude, altitude, &status);
    ASSERT_EQ(0, status) << oskar_get_error_string(status);
    for (int i = 0; i < n_stations; ++i)
    {
        sprintf(station_name, "%s_%d.dat", station_base, i);
        oskar_station_model_load_config(&(tel_cpu.station[i]), station_name,
                &status);
        ASSERT_EQ(0, status) << oskar_get_error_string(status);
        EXPECT_EQ(n_elements, tel_cpu.station[i].num_elements);
    }

    // Copy telescope structure to GPU.
    oskar_telescope_model_init_copy(&tel_gpu, &tel_cpu,
            OSKAR_LOCATION_GPU, &status);
    ASSERT_EQ(0, status) << oskar_get_error_string(status);

    // Copy the telescope structure back to the CPU.
    oskar_telescope_model_init_copy(&tel_cpu2, &tel_gpu,
            OSKAR_LOCATION_CPU, &status);
    ASSERT_EQ(0, status) << oskar_get_error_string(status);

    // Check the contents of the CPU structure.
    for (int i = 0; i < n_stations; ++i)
    {
        // Define horizon coordinates.
        double x_hor = i / 10.0;
        double y_hor = i / 20.0;
        double z_hor = i / 30.0;

        // Compute offset geocentric coordinates.
        double x = 0.0, y = 0.0, z = 0.0;
        oskar_horizon_plane_to_offset_geocentric_cartesian_d(1,
                &x_hor, &y_hor, &z_hor, longitude, latitude, &x, &y, &z);
        EXPECT_NEAR(x, ((double*)(tel_cpu2.station_x))[i], 1e-5);
        EXPECT_NEAR(y, ((double*)(tel_cpu2.station_y))[i], 1e-5);
        EXPECT_NEAR(z, ((double*)(tel_cpu2.station_z))[i], 1e-5);

        for (int j = 0; j < n_elements; ++j)
        {
            int t = j + i;
            double x = t / 5.0;
            double y = t / 6.0;
            double z = t / 7.0;
            EXPECT_NEAR(x, ((double*)(tel_cpu2.station[i].x_weights))[j], 1e-5);
            EXPECT_NEAR(y, ((double*)(tel_cpu2.station[i].y_weights))[j], 1e-5);
            EXPECT_NEAR(z, ((double*)(tel_cpu2.station[i].z_weights))[j], 1e-5);
        }
    }

    // Free memory.
    oskar_telescope_model_free(&tel_cpu, &status);
    oskar_telescope_model_free(&tel_cpu2, &status);
    oskar_telescope_model_free(&tel_gpu, &status);
    ASSERT_EQ(0, status) << oskar_get_error_string(status);

    delete_test_data();
}
