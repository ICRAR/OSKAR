/*
 * Copyright (c) 2015, The University of Oxford
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

#include <oskar_get_memory_usage.h>

#include <stdio.h>

#if defined(OSKAR_OS_LINUX)
#   include <sys/types.h>
#   include <sys/sysinfo.h>
#elif defined(OSKAR_OS_MAC)
#   include <sys/param.h>
#   include <sys/mount.h>
#   include <sys/types.h>
#   include <sys/sysctl.h>
#   include <mach/mach.h>
#   include <mach/vm_statistics.h>
#   include <mach/mach_types.h>
#   include <mach/mach_init.h>
#   include <mach/mach_host.h>
#   include <stddef.h>
#   include <sys/sysctl.h>
#elif defined(OSKAR_OS_WIN)
#   include <windows.h>
#endif

/* http://goo.gl/iKnmd */

#ifdef __cplusplus
extern "C" {
#endif

long long oskar_get_total_physical_memory(void)
{
#ifdef OSKAR_OS_LINUX
    long long totalPhysMem = 0;
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    totalPhysMem = memInfo.totalram;
    totalPhysMem *= memInfo.mem_unit;
    return totalPhysMem;
#elif defined(OSKAR_OS_MAC)
    int mib[2];
    int64_t physical_memory;
    size_t length;
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    sysctl(mib, 2, &physical_memory, &length, NULL, 0);
    return (long long)physical_memory;
#elif defined(OSKAR_OS_WIN)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    return (long long)totalPhysMem;
#endif
}

long long oskar_get_free_physical_memory(void)
{
#ifdef OSKAR_OS_LINUX
    long long freePhysMem = 0;
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    freePhysMem = memInfo.freeram;
    freePhysMem *= memInfo.mem_unit;
    return freePhysMem;
#elif defined(OSKAR_OS_MAC)
    vm_size_t page_size;
    mach_port_t mach_port;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;
    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
            KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                    (host_info64_t)&vm_stats, &count))
    {
        /* Note on OS X since 10.9, compressed memory makes this value somewhat
         * flexible. */
        return (long long)((int64_t)vm_stats.free_count*(int64_t)page_size);
    }
    return -1;
#elif defined(OSKAR_OS_WIN)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG freePhysMem = memInfo.ullAvailPhys;
    return (long long)freePhysMem;
#endif
}

long long oskar_get_total_virtual_memory(void)
{
#ifdef OSKAR_OS_LINUX
    struct sysinfo memInfo;
    long long totalVirtualMem;
    sysinfo(&memInfo);
    totalVirtualMem = memInfo.totalram;
    totalVirtualMem += memInfo.totalswap;
    totalVirtualMem *= memInfo.mem_unit;
    return totalVirtualMem;
#elif defined(OSKAR_OS_MAC)
    struct statfs stats;
    if (0 == statfs("/", &stats))
    {
        uint64_t myFreeSwap = (uint64_t)stats.f_bsize * stats.f_bfree;
        return myFreeSwap;
    }
    return -1;
#elif defined(OSKAR_OS_WIN)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return (long long)memInfo.ullTotalPageFile;
#endif
}

long long oskar_get_free_virtual_memory(void)
{
#ifdef OSKAR_OS_LINUX
    struct sysinfo memInfo;
    long long freeVirtualMem;
    sysinfo(&memInfo);
    freeVirtualMem = memInfo.freeram;
    freeVirtualMem += memInfo.freeswap;
    freeVirtualMem *= memInfo.mem_unit;
    return freeVirtualMem;
#elif defined(OSKAR_OS_MAC)
    struct xsw_usage vmusage;
    size_t size = sizeof(vmusage);
    if (sysctlbyname("vm.swapusage", &vmusage, &size, NULL, 0)!=0)
        return -1;
    return (long long)vmusage.xsu_avail;
#elif defined(OSKAR_OS_WIN)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return (long long)memInfo.ullAvailPageFile;
#endif
}

void oskar_print_memory_info(void)
{
    long long totalVirtualMem, freeVirtualMem, totalPhysMem, freePhysMem;
    totalPhysMem = oskar_get_total_physical_memory();
    freePhysMem = oskar_get_free_physical_memory();
    totalVirtualMem = oskar_get_total_virtual_memory();
    freeVirtualMem = oskar_get_free_virtual_memory();
    printf("Total physical memory = %lld MB\n", totalPhysMem/(1024*1024));
    printf("Free physical memory  = %lld MB\n", freePhysMem/(1024*1024));
    printf("Total virtual memory  = %lld MB\n", totalVirtualMem/(1024*1024));
    printf("Free virtual memory   = %lld MB\n", freeVirtualMem/(1024*1024));
}

#ifdef __cplusplus
}
#endif
