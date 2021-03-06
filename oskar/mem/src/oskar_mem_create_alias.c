/*
 * Copyright (c) 2014-2017, The University of Oxford
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

#include "mem/oskar_mem.h"
#include "mem/private_mem.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

oskar_Mem* oskar_mem_create_alias(const oskar_Mem* src, size_t offset,
        size_t num_elements, int* status)
{
    oskar_Mem* mem = 0;

    /* Create the structure, initialised with all bits zero. */
    mem = (oskar_Mem*) calloc(1, sizeof(oskar_Mem));
    if (!mem)
    {
        *status = OSKAR_ERR_MEMORY_ALLOC_FAILURE;
        return 0;
    }

    /* Initialise meta-data.
     * (This must happen regardless of the status code.) */
    mem->owner = 0; /* Structure does not own the memory. */
    if (src)
    {
        size_t element_size = oskar_mem_element_size(src->type);
        mem->type = src->type;
        mem->location = src->location;
        mem->num_elements = num_elements;
#ifdef OSKAR_HAVE_OPENCL
        if (mem->location & OSKAR_CL)
        {
            cl_int error = 0;
            cl_buffer_region r;
            r.origin = element_size * offset;
            r.size   = element_size * num_elements;
            mem->buffer = clCreateSubBuffer(src->buffer,
                    CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                    &r, &error);
            if (error != CL_SUCCESS)
                *status = OSKAR_ERR_MEMORY_ALLOC_FAILURE;
        }
        else
#endif
        {
            size_t offset_bytes = offset * element_size;
            mem->data = (void*)(((char*)(src->data)) + offset_bytes);
        }
    }

    /* Return a handle to the new structure. */
    return mem;
}

#ifdef __cplusplus
}
#endif
