/*
 * Copyright (c) 2016, The University of Oxford
 * All rights reserved.
 *
 * This file is part of the OSKAR package.
 * Contact: oskar at oerc.ox.ac.uk
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

#include <Python.h>

#include <oskar_Settings_old.h>
#include <oskar_settings_load.h>
#include <oskar_settings_old_free.h>
#include <oskar_sky.h>
#include <oskar_set_up_sky.h>
#include <oskar_get_error_string.h>
#include <string.h>

/* http://docs.scipy.org/doc/numpy-dev/reference/c-api.deprecations.html */
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

static const char* module_doc =
        "This module provides an interface to the OSKAR sky model.";
static const char* name = "oskar_Sky";

static void sky_free(PyObject* capsule)
{
    int status = 0;
    oskar_Sky* h = (oskar_Sky*) PyCapsule_GetPointer(capsule, name);
    oskar_sky_free(h, &status);
}


static oskar_Sky* get_handle(PyObject* capsule)
{
    oskar_Sky* h = 0;
    if (!PyCapsule_CheckExact(capsule))
    {
        PyErr_SetString(PyExc_RuntimeError, "Input is not a PyCapsule object!");
        return 0;
    }
    h = (oskar_Sky*) PyCapsule_GetPointer(capsule, name);
    if (!h)
    {
        PyErr_SetString(PyExc_RuntimeError,
                "Unable to convert PyCapsule object to pointer.");
        return 0;
    }
    return h;
}


static int numpy_type_from_oskar(int type)
{
    switch (type)
    {
    case OSKAR_INT:            return NPY_INT;
    case OSKAR_SINGLE:         return NPY_FLOAT;
    case OSKAR_DOUBLE:         return NPY_DOUBLE;
    case OSKAR_SINGLE_COMPLEX: return NPY_CFLOAT;
    case OSKAR_DOUBLE_COMPLEX: return NPY_CDOUBLE;
    }
    return 0;
}


static PyObject* append(PyObject* self, PyObject* args)
{
    oskar_Sky *h1 = 0, *h2 = 0;
    PyObject *capsule1 = 0, *capsule2 = 0;
    int status = 0;
    if (!PyArg_ParseTuple(args, "OO", &capsule1, &capsule2)) return 0;
    if (!(h1 = get_handle(capsule1))) return 0;
    if (!(h2 = get_handle(capsule2))) return 0;

    /* Append the sky model. */
    oskar_sky_append(h1, h2, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "oskar_sky_append() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        return 0;
    }
    return Py_BuildValue("");
}


static PyObject* append_sources(PyObject* self, PyObject* args)
{
    oskar_Sky *h = 0;
    PyObject *obj[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    oskar_Mem *ra_c, *dec_c, *I_c, *Q_c, *U_c, *V_c;
    oskar_Mem *ref_c, *spix_c, *rm_c, *maj_c, *min_c, *pa_c;
    PyArrayObject *ra = 0, *dec = 0, *I = 0, *Q = 0, *U = 0, *V = 0;
    PyArrayObject *ref = 0, *spix = 0, *rm = 0, *maj = 0, *min = 0, *pa = 0;
    int status = 0, npy_type, type, flags, num_sources, old_num;

    /* Parse inputs: RA, Dec, I, Q, U, V, ref, spix, rm, maj, min, pa. */
    if (!PyArg_ParseTuple(args, "OOOOOOOOOOOOO", &obj[0],
            &obj[1], &obj[2], &obj[3], &obj[4], &obj[5], &obj[6],
            &obj[7], &obj[8], &obj[9], &obj[10], &obj[11], &obj[12]))
        return 0;
    if (!(h = get_handle(obj[0]))) return 0;

    /* Make sure input objects are arrays. Convert if required. */
    flags = NPY_ARRAY_FORCECAST | NPY_ARRAY_IN_ARRAY;
    type = oskar_sky_precision(h);
    npy_type = numpy_type_from_oskar(type);
    ra   = (PyArrayObject*) PyArray_FROM_OTF(obj[1], npy_type, flags);
    dec  = (PyArrayObject*) PyArray_FROM_OTF(obj[2], npy_type, flags);
    I    = (PyArrayObject*) PyArray_FROM_OTF(obj[3], npy_type, flags);
    Q    = (PyArrayObject*) PyArray_FROM_OTF(obj[4], npy_type, flags);
    U    = (PyArrayObject*) PyArray_FROM_OTF(obj[5], npy_type, flags);
    V    = (PyArrayObject*) PyArray_FROM_OTF(obj[6], npy_type, flags);
    ref  = (PyArrayObject*) PyArray_FROM_OTF(obj[7], npy_type, flags);
    spix = (PyArrayObject*) PyArray_FROM_OTF(obj[8], npy_type, flags);
    rm   = (PyArrayObject*) PyArray_FROM_OTF(obj[9], npy_type, flags);
    maj  = (PyArrayObject*) PyArray_FROM_OTF(obj[10], npy_type, flags);
    min  = (PyArrayObject*) PyArray_FROM_OTF(obj[11], npy_type, flags);
    pa   = (PyArrayObject*) PyArray_FROM_OTF(obj[12], npy_type, flags);
    if (!ra || !dec || !I || !Q || !U || !V ||
            !ref || !spix || !rm || !maj || !min || !pa)
        goto fail;

    /* Check size of input arrays. */
    num_sources = (int) PyArray_SIZE(I);
    if (num_sources != (int) PyArray_SIZE(ra) ||
            num_sources != (int) PyArray_SIZE(dec) ||
            num_sources != (int) PyArray_SIZE(Q) ||
            num_sources != (int) PyArray_SIZE(U) ||
            num_sources != (int) PyArray_SIZE(V) ||
            num_sources != (int) PyArray_SIZE(ref) ||
            num_sources != (int) PyArray_SIZE(spix) ||
            num_sources != (int) PyArray_SIZE(rm) ||
            num_sources != (int) PyArray_SIZE(maj) ||
            num_sources != (int) PyArray_SIZE(min) ||
            num_sources != (int) PyArray_SIZE(pa))
    {
        PyErr_SetString(PyExc_RuntimeError, "Input data dimension mismatch.");
        goto fail;
    }

    /* Pointers to input arrays. */
    ra_c = oskar_mem_create_alias_from_raw(PyArray_DATA(ra),
            type, OSKAR_CPU, num_sources, &status);
    dec_c = oskar_mem_create_alias_from_raw(PyArray_DATA(dec),
            type, OSKAR_CPU, num_sources, &status);
    I_c = oskar_mem_create_alias_from_raw(PyArray_DATA(I),
            type, OSKAR_CPU, num_sources, &status);
    Q_c = oskar_mem_create_alias_from_raw(PyArray_DATA(Q),
            type, OSKAR_CPU, num_sources, &status);
    U_c = oskar_mem_create_alias_from_raw(PyArray_DATA(U),
            type, OSKAR_CPU, num_sources, &status);
    V_c = oskar_mem_create_alias_from_raw(PyArray_DATA(V),
            type, OSKAR_CPU, num_sources, &status);
    ref_c = oskar_mem_create_alias_from_raw(PyArray_DATA(ref),
            type, OSKAR_CPU, num_sources, &status);
    spix_c = oskar_mem_create_alias_from_raw(PyArray_DATA(spix),
            type, OSKAR_CPU, num_sources, &status);
    rm_c = oskar_mem_create_alias_from_raw(PyArray_DATA(rm),
            type, OSKAR_CPU, num_sources, &status);
    maj_c = oskar_mem_create_alias_from_raw(PyArray_DATA(maj),
            type, OSKAR_CPU, num_sources, &status);
    min_c = oskar_mem_create_alias_from_raw(PyArray_DATA(min),
            type, OSKAR_CPU, num_sources, &status);
    pa_c = oskar_mem_create_alias_from_raw(PyArray_DATA(pa),
            type, OSKAR_CPU, num_sources, &status);

    /* Copy source data into the sky model. */
    old_num = oskar_sky_num_sources(h);
    oskar_sky_resize(h, old_num + num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_ra_rad(h), ra_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_dec_rad(h), dec_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_I(h), I_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_Q(h), Q_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_U(h), U_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_V(h), V_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_reference_freq_hz(h), ref_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_spectral_index(h), spix_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_rotation_measure_rad(h), rm_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_fwhm_major_rad(h), maj_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_fwhm_minor_rad(h), min_c,
            old_num, 0, num_sources, &status);
    oskar_mem_copy_contents(oskar_sky_position_angle_rad(h), pa_c,
            old_num, 0, num_sources, &status);

    /* Free memory. */
    oskar_mem_free(ra_c, &status);
    oskar_mem_free(dec_c, &status);
    oskar_mem_free(I_c, &status);
    oskar_mem_free(Q_c, &status);
    oskar_mem_free(U_c, &status);
    oskar_mem_free(V_c, &status);
    oskar_mem_free(ref_c, &status);
    oskar_mem_free(spix_c, &status);
    oskar_mem_free(rm_c, &status);
    oskar_mem_free(maj_c, &status);
    oskar_mem_free(min_c, &status);
    oskar_mem_free(pa_c, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "Sky model append_sources() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        goto fail;
    }

    Py_XDECREF(ra);
    Py_XDECREF(dec);
    Py_XDECREF(I);
    Py_XDECREF(Q);
    Py_XDECREF(U);
    Py_XDECREF(V);
    Py_XDECREF(ref);
    Py_XDECREF(spix);
    Py_XDECREF(rm);
    Py_XDECREF(maj);
    Py_XDECREF(min);
    Py_XDECREF(pa);
    return Py_BuildValue("");

fail:
    Py_XDECREF(ra);
    Py_XDECREF(dec);
    Py_XDECREF(I);
    Py_XDECREF(Q);
    Py_XDECREF(U);
    Py_XDECREF(V);
    Py_XDECREF(ref);
    Py_XDECREF(spix);
    Py_XDECREF(rm);
    Py_XDECREF(maj);
    Py_XDECREF(min);
    Py_XDECREF(pa);
    return 0;
}


static PyObject* append_file(PyObject* self, PyObject* args)
{
    oskar_Sky *h = 0, *t = 0;
    PyObject* capsule = 0;
    int status = 0;
    const char* filename = 0;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &filename)) return 0;
    if (!(h = get_handle(capsule))) return 0;

    /* Load the sky model. */
    t = oskar_sky_load(filename, oskar_sky_precision(h), &status);
    oskar_sky_append(h, t, &status);
    oskar_sky_free(t, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "oskar_sky_load() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        return 0;
    }
    return Py_BuildValue("");
}


static PyObject* create(PyObject* self, PyObject* args)
{
    oskar_Sky* h = 0;
    PyObject* capsule = 0;
    int status = 0, prec = 0;
    const char* type;
    if (!PyArg_ParseTuple(args, "s", &type)) return 0;
    prec = (type[0] == 'S' || type[0] == 's') ? OSKAR_SINGLE : OSKAR_DOUBLE;
    h = oskar_sky_create(prec, OSKAR_CPU, 0, &status);
    capsule = PyCapsule_New((void*)h, name,
            (PyCapsule_Destructor)sky_free);
    return Py_BuildValue("N", capsule); /* Don't increment refcount. */
}


static PyObject* generate_grid(PyObject* self, PyObject* args)
{
    oskar_Sky *h = 0;
    PyObject* capsule = 0;
    int prec, side_length = 0, seed = 1, status = 0;
    const char* type = 0;
    double ra0, dec0, fov, mean_flux_jy, std_flux_jy;
    if (!PyArg_ParseTuple(args, "ddidddis", &ra0, &dec0, &side_length,
            &fov, &mean_flux_jy, &std_flux_jy, &seed, &type)) return 0;

    /* Generate the grid. */
    prec = (type[0] == 'S' || type[0] == 's') ? OSKAR_SINGLE : OSKAR_DOUBLE;
    ra0 *= M_PI / 180.0;
    dec0 *= M_PI / 180.0;
    fov *= M_PI / 180.0;
    h = oskar_sky_generate_grid(prec, ra0, dec0, side_length, fov,
            mean_flux_jy, std_flux_jy, seed, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "oskar_sky_generate_grid() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        oskar_sky_free(h, &status);
        return 0;
    }
    capsule = PyCapsule_New((void*)h, name,
            (PyCapsule_Destructor)sky_free);
    return Py_BuildValue("N", capsule); /* Don't increment refcount. */
}


static PyObject* generate_random_power_law(PyObject* self, PyObject* args)
{
    oskar_Sky *h = 0;
    PyObject* capsule = 0;
    int prec, num_sources = 0, seed = 1, status = 0;
    const char* type = 0;
    double min_flux_jy = 0.0, max_flux_jy = 0.0, power = 0.0;
    if (!PyArg_ParseTuple(args, "idddis", &num_sources, &min_flux_jy,
            &max_flux_jy, &power, &seed, &type)) return 0;

    /* Generate the sources. */
    prec = (type[0] == 'S' || type[0] == 's') ? OSKAR_SINGLE : OSKAR_DOUBLE;
    h = oskar_sky_generate_random_power_law(prec, num_sources,
            min_flux_jy, max_flux_jy, power, seed, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "oskar_sky_generate_random_power_law() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        oskar_sky_free(h, &status);
        return 0;
    }
    capsule = PyCapsule_New((void*)h, name,
            (PyCapsule_Destructor)sky_free);
    return Py_BuildValue("N", capsule); /* Don't increment refcount. */
}


static PyObject* save(PyObject* self, PyObject* args)
{
    oskar_Sky *h = 0;
    PyObject* capsule = 0;
    int status = 0;
    const char* filename = 0;
    if (!PyArg_ParseTuple(args, "Os", &capsule, &filename)) return 0;
    if (!(h = get_handle(capsule))) return 0;

    /* Save the sky model. */
    oskar_sky_save(filename, h, &status);

    /* Check for errors. */
    if (status)
    {
        PyErr_Format(PyExc_RuntimeError,
                "oskar_sky_save() failed with code %d (%s).",
                status, oskar_get_error_string(status));
        return 0;
    }
    return Py_BuildValue("");
}


static PyObject* set_up(PyObject* self, PyObject* args)
{
    oskar_Sky* h = 0;
    PyObject* capsule = 0;
    oskar_Settings_old s_old;
    const char* filename = 0;
    int status = 0;

    /* Load the settings file. */
    /* FIXME Stop using the old settings structures. */
    if (!PyArg_ParseTuple(args, "s", &filename)) return 0;
    oskar_settings_old_load(&s_old, 0, filename, &status);

    /* Check for errors. */
    if (status)
    {
        oskar_settings_old_free(&s_old);
        PyErr_Format(PyExc_RuntimeError,
                "Unable to load settings file (%s).",
                oskar_get_error_string(status));
        return 0;
    }

    /* Set up the sky model. */
    h = oskar_set_up_sky(&s_old, 0, &status);

    /* Check for errors. */
    if (status || !h)
    {
        oskar_settings_old_free(&s_old);
        oskar_sky_free(h, &status);
        PyErr_Format(PyExc_RuntimeError,
                "Sky model set up failed with code %d (%s).",
                status, oskar_get_error_string(status));
        return 0;
    }

    /* Create the PyCapsule and return it. */
    oskar_settings_old_free(&s_old);
    capsule = PyCapsule_New((void*)h, name,
            (PyCapsule_Destructor)sky_free);
    return Py_BuildValue("N", capsule); /* Don't increment refcount. */
}


/* Method table. */
static PyMethodDef methods[] =
{
        {"create", (PyCFunction)create, METH_VARARGS, "create(type)"},
        {"append", (PyCFunction)append, METH_VARARGS, "append(sky)"},
        {"append_sources", (PyCFunction)append_sources, METH_VARARGS,
                "append_sources(ra, dec, I, Q, U, V, ref_freq, spectral_index, "
                "rotation_measure, major, minor, position_angle)"},
        {"append_file", (PyCFunction)append_file, METH_VARARGS,
                "append_file(filename)"},
        {"save", (PyCFunction)save, METH_VARARGS, "save(filename)"},
        {"generate_grid", (PyCFunction)generate_grid, METH_VARARGS,
                "generate_grid()"},
        {"generate_random_power_law", (PyCFunction)generate_random_power_law,
                METH_VARARGS, "generate_random_power_law(num_sources, "
                        "min_flux_jy, max_flux_jy, power, seed, type)"},
        {"set_up", (PyCFunction)set_up, METH_VARARGS, "set_up(settings_path)"},
        {NULL, NULL, 0, NULL}
};


#if PY_MAJOR_VERSION >= 3
static PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_sky_lib",         /* m_name */
        module_doc,         /* m_doc */
        -1,                 /* m_size */
        methods             /* m_methods */
};
#endif


static PyObject* moduleinit(void)
{
    PyObject* m;
#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule3("_sky_lib", methods, module_doc);
#endif
    return m;
}

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit__sky_lib(void)
{
    import_array();
    return moduleinit();
}
#else
/* The init function name has to match that of the compiled module
 * with the pattern 'init<module name>'. This module is called '_sky_lib' */
PyMODINIT_FUNC init_sky_lib(void)
{
    import_array();
    moduleinit();
    return;
}
#endif
