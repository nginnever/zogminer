/* MIT License
 *
 * Copyright (c) 2016 Nathan Ginnever
 * Copyright (c) 2016 Omar Alvarez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _XOPEN_SOURCE 700
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sodium.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <math.h>
#include <cl.h>


#define EQUIHASH_N 200
#define EQUIHASH_K 9

#define NUM_COLLISION_BITS (EQUIHASH_N / (EQUIHASH_K + 1))
#define NUM_INDICES (1 << EQUIHASH_K)

#define NUM_COMPRESSED_INDICE_BITS 16
#define NUM_DECOMPRESSED_INDICE_BITS (NUM_COLLISION_BITS+1)

#define NUM_INDICE_BYTES_PER_ELEMENT (((NUM_INDICES/2) * NUM_COMPRESSED_INDICE_BITS + 7) / 8)
#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define DIGEST_SIZE 32

typedef crypto_generichash_blake2b_state eh_HashState;
typedef uint32_t eh_index;

typedef struct element {
    uint8_t digest[DIGEST_SIZE];
    uint16_t indices[NUM_INDICE_BYTES_PER_ELEMENT / 8];
} element_t;

uint32_t bswap_32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >>  8) |
    ((x & 0x0000ff00U) << 8) | ((x & 0x000000ffU) << 24));
}

uint32_t htole32(uint32_t little_endian_bits)
{
    return bswap_32(little_endian_bits);
}

void hexout(unsigned char* digest_result) {
    for(unsigned i = 0; i < 4; ++i) {
        for(int j = 0; j < 8; ++j) {
            int c = digest_result[i*8 + j];
            printf("%2X", c);
        }
    }
    printf("\n");
}

typedef struct gpu_config {
    unsigned flags;

    char* program_source_code;
    size_t program_source_code_size;

    cl_program program;

    cl_platform_id platform_ids;
    cl_uint n_platforms;

    cl_device_id device_ids;
    cl_uint n_devices;

    cl_context context;
    cl_command_queue command_queue;


    cl_kernel initial_hashing_kernel;
    cl_kernel collide_kernel;
    cl_kernel produce_solutions_kernel;

    // gpu variables below
    cl_mem src;
    cl_mem n_first_counter;
    cl_mem n_last_counter;
    cl_mem n_dst_elements;
    cl_mem blake2b_digest;
    cl_mem n_solutions;
    cl_mem dst_solutions;

    // size variables
    cl_ulong max_bytes;
} gpu_config_t;

void init_program(gpu_config_t* config, const char* file_path, unsigned flags) {
    memset(config, '\0', sizeof(gpu_config_t));
    
    config->flags = flags;

    FILE* f = fopen(file_path, "r");
    if (!f) {
        fprintf(stderr, "program with path \"%s\".\n", file_path);
        exit(1);
    }
    config->program_source_code = calloc(400000, sizeof(char));
    config->program_source_code_size = fread(config->program_source_code, 1, 400000, f);
    fclose(f); 

    cl_int ret = 0;
    cl_int zero = 0;

    clGetPlatformIDs(1, &config->platform_ids, &config->n_platforms);
    clGetDeviceIDs(config->platform_ids, CL_DEVICE_TYPE_DEFAULT, 1, &config->device_ids, &config->n_devices);
    config->context = clCreateContext(NULL, 1, &config->device_ids, NULL, NULL, &ret);

    config->program = clCreateProgramWithSource(config->context, 1, &config->program_source_code, &config->program_source_code_size, &ret);


    cl_build_status status;
    cl_int err;
    size_t logSize;
    char *programLog;
    //check_error(clBuildProgram(config->program, 1, &config->device_ids, NULL, NULL, NULL), __LINE__);
    if (clBuildProgram(config->program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
    {
        // check build error and build status first
        clGetProgramBuildInfo(config->program, config->device_ids, CL_PROGRAM_BUILD_STATUS,
                              sizeof(cl_build_status), &status, NULL);
        
        // check build log
        clGetProgramBuildInfo(config->program, config->device_ids,
                              CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        programLog = (char*) calloc (logSize+1, sizeof(char));
        clGetProgramBuildInfo(config->program, config->device_ids,
                              CL_PROGRAM_BUILD_LOG, logSize+1, programLog, NULL);
        printf("Build failed; error=%d, status=%d, programLog:nn%s",
               err, status, programLog);
        free(programLog);
        
        //return 1;
    }
    config->initial_hashing_kernel = clCreateKernel(config->program, "initial_hashing", &ret);
    config->collide_kernel = clCreateKernel(config->program, "collide", &ret);

    config->produce_solutions_kernel = clCreateKernel(config->program, "produce_solutions", &ret);

    config->command_queue = clCreateCommandQueue(config->context, config->device_ids, CL_QUEUE_PROFILING_ENABLE, &ret);

    config->src = clCreateBuffer(config->context, CL_MEM_READ_WRITE, NUM_VALUES * sizeof(element_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->src, &zero, 1, 0, NUM_VALUES * sizeof(element_t), 0, NULL, NULL);

    config->n_first_counter = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->n_first_counter, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL);

    config->n_last_counter = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->n_last_counter, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL);

    config->n_dst_elements = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->n_dst_elements, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL);

    config->blake2b_digest = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(crypto_generichash_blake2b_state), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->blake2b_digest, &zero, 1, 0, sizeof(crypto_generichash_blake2b_state), 0, NULL, NULL);

    config->dst_solutions = clCreateBuffer(config->context, CL_MEM_READ_WRITE, 20*NUM_INDICES*sizeof(uint32_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->dst_solutions, &zero, 1, 0, 20*NUM_INDICES*sizeof(uint32_t), 0, NULL, NULL);


    config->n_solutions = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    clEnqueueFillBuffer(config->command_queue, config->n_solutions, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL);

    printf("approx memory needed for solver: %zu bytes\n", sizeof(element_t) * NUM_VALUES);
    clGetDeviceInfo(config->device_ids, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(config->max_bytes), &config->max_bytes, &ret);
    printf("device max memory: %u bytes\n", config->max_bytes);
    printf("device max alloc allowed: %u bytes\n", config->max_bytes/4);

    free(config->program_source_code);
    
}

void cleanup_program(gpu_config_t* config) {
    clReleaseProgram(config->program);
    clReleaseKernel(config->initial_hashing_kernel);
    clReleaseKernel(config->collide_kernel);
    clReleaseKernel(config->produce_solutions_kernel);
    clReleaseCommandQueue(config->command_queue);


    clReleaseMemObject(config->src);
    clReleaseMemObject(config->n_dst_elements);
    clReleaseMemObject(config->n_solutions);
    clReleaseMemObject(config->blake2b_digest);
    clReleaseMemObject(config->dst_solutions);
    clReleaseContext(config->context);
}


// cpu sort for now while i work on radix sort
int compare_elements(const void* a, const void* b) {
    return memcmp(a, b, DIGEST_SIZE);
}


size_t equihash(uint32_t* dst_solutions, crypto_generichash_blake2b_state* digest) {
    size_t global_work_offset = 0;
    size_t global_work_size = 1 << 10;
    size_t local_work_size = 256;
    gpu_config_t config;
    init_program(&config, "./equihash.cl", 0);
    
    cl_ulong time_start;
    cl_ulong time_end;
    cl_ulong total_time = 0;
    cl_event timing_events[20];
    cl_int zero = 0;

    clEnqueueWriteBuffer(config.command_queue, config.blake2b_digest, CL_TRUE, 0, sizeof(crypto_generichash_blake2b_state), (void*)digest, 0, NULL, NULL);


    clSetKernelArg(config.initial_hashing_kernel, 0, sizeof(cl_mem), (void *)&config.src);
    clSetKernelArg(config.initial_hashing_kernel, 1, sizeof(cl_mem), (void *)&config.blake2b_digest);
    clSetKernelArg(config.initial_hashing_kernel, 2, sizeof(cl_mem), (void *)&config.n_first_counter);
    clEnqueueNDRangeKernel(config.command_queue, config.initial_hashing_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[0]);
    clWaitForEvents(1, &timing_events[0]);
    clGetEventProfilingInfo(timing_events[0], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
    clGetEventProfilingInfo(timing_events[0], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
    fprintf(stderr, "step0: %0.3f ms\n", (time_end - time_start) / 1000000.0);
    total_time += (time_end-time_start);

    uint32_t n_src_elements = NUM_VALUES;


    // TO ACTUALLY MAKE THIS ALGO WORK EFFICIENTLY ON A GPU YOU NEED TO UNPLUG THIS CODE AND PLUG IN YOUR OWN SORTING KERNEL
    // CPU SORTING CODE
    element_t* cpu_src = calloc(n_src_elements, sizeof(element_t));
    clEnqueueReadBuffer(config.command_queue, config.src, CL_TRUE, 0, n_src_elements*sizeof(element_t), cpu_src, 0, NULL, NULL);
    qsort(cpu_src, n_src_elements, sizeof(element_t), compare_elements);

    // for(int y = 0; y < 10; y++){
    //     //printf("%u, %i\n", cpu_src[y].hash[10], cpu_src[y].index[0]);
    //         element_t* elem_ptr = &cpu_src[y*sizeof(element_t)];
    //         for(int z =0; z<32; z++){
    //             printf("%u", elem_ptr->digest[z]);
    //             //printf("%u", elem_ptr.hash[z]);
    //         }
    //         printf(", %u\n", elem_ptr->indices[0]);
    //         //elem_ptr->index = i + j;
    // }

    //clEnqueueWriteBuffer(config.command_queue, config.src, CL_TRUE, 0, n_src_elements*sizeof(element_t), (void*)cpu_src, 0, NULL, NULL);
    free(cpu_src);

    uint32_t i = 1;
    for(i = 1; i < EQUIHASH_K; ++i) {
        fprintf(stderr, "%u\n", n_src_elements);
        clEnqueueFillBuffer(config.command_queue, config.n_dst_elements, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL);
        clEnqueueWriteBuffer(config.command_queue, config.n_last_counter, CL_TRUE, 0, sizeof(uint32_t), (void*)&n_src_elements, 0, NULL, NULL);
        clSetKernelArg(config.collide_kernel, 0, sizeof(cl_mem), (void *)&config.src);
        clSetKernelArg(config.collide_kernel, 1, sizeof(cl_mem), (void *)&config.n_dst_elements);
        clSetKernelArg(config.collide_kernel, 2, sizeof(cl_mem), (void *)&config.n_last_counter);
        clSetKernelArg(config.collide_kernel, 3, sizeof(uint32_t), (void *)&n_src_elements);
        clSetKernelArg(config.collide_kernel, 4, sizeof(uint32_t), (void*)&i);
        clEnqueueNDRangeKernel(config.command_queue, config.collide_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[i]);
        clWaitForEvents(1, &timing_events[i]);
        clGetEventProfilingInfo(timing_events[i], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
        clGetEventProfilingInfo(timing_events[i], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
        uint32_t n_old_src_elements = n_src_elements;
        clEnqueueReadBuffer(config.command_queue, config.n_dst_elements, CL_TRUE, 0, sizeof(uint32_t), &n_src_elements, 0, NULL, NULL);
        fprintf(stderr, "step %u: %0.3f ms (%u)\n", i, (time_end - time_start) / 1000000.0, n_src_elements);
        total_time += (time_end-time_start);

        // TO ACTUALLY MAKE THIS ALGO WORK EFFICIENTLY ON A GPU YOU NEED TO UNPLUG THIS CODE AND PLUG IN YOUR OWN SORTING KERNEL
        // CPU SORTING CODE
        element_t* cpu_src = calloc(n_src_elements, sizeof(element_t));
        clEnqueueReadBuffer(config.command_queue, config.src, CL_TRUE, n_old_src_elements*sizeof(element_t), n_src_elements*sizeof(element_t), cpu_src, 0, NULL, NULL);
        qsort(cpu_src, n_src_elements, sizeof(element_t), compare_elements);
        clEnqueueWriteBuffer(config.command_queue, config.src, CL_TRUE, 0, n_src_elements*sizeof(element_t), (void*)cpu_src, 0, NULL, NULL);
        free(cpu_src);
    }


    uint32_t n_solutions = 0;
    clSetKernelArg(config.produce_solutions_kernel, 0, sizeof(cl_mem), (void *)&config.dst_solutions);
    clSetKernelArg(config.produce_solutions_kernel, 1, sizeof(cl_mem), (void *)&config.n_solutions);
    clSetKernelArg(config.produce_solutions_kernel, 2, sizeof(cl_mem), (void *)&config.src);
    clSetKernelArg(config.produce_solutions_kernel, 3, sizeof(uint32_t), (void *)&n_src_elements);
    clSetKernelArg(config.produce_solutions_kernel, 4, sizeof(cl_mem), (void *)&config.blake2b_digest);
    clEnqueueNDRangeKernel(config.command_queue, config.produce_solutions_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[9]);
    clWaitForEvents(1, &timing_events[9]);
    clGetEventProfilingInfo(timing_events[9], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
    clGetEventProfilingInfo(timing_events[9], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
    fprintf(stderr, "step9: %0.3f ms\n", (time_end - time_start) / 1000000.0);
    total_time += (timze_end-time_start);

    clEnqueueReadBuffer(config.command_queue, config.dst_solutions, CL_TRUE, 0, 10*NUM_INDICES*sizeof(uint32_t), dst_solutions, 0, NULL, NULL);
    clEnqueueReadBuffer(config.command_queue, config.n_solutions, CL_TRUE, 0, sizeof(uint32_t), &n_solutions, 0, NULL, NULL);

    fprintf(stderr, "found %u solutions in %0.3f ms\n", n_solutions, total_time / 1000000.0);

    return n_solutions;
}

int main(void)
{
    printf("initializing...\n");
    cl_mem dst_solutions;
    eh_HashState state;
    uint8_t hash[DIGEST_SIZE];
    uint32_t nonce = 420;
    eh_index lei = htole32(nonce);
    //crypto_generichash_blake2b_update(&state, (const unsigned char*) &lei, sizeof(eh_index));
    //crypto_generichash_blake2b_final(&state, hash, 50);

    uint32_t solns[60];
    equihash(&solns, &state);

    return 0;
}
