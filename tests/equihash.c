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
#include <CL/cl.h>

double get_ttime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}


// COMPILE INSTRUCTIONS:
// gcc-4.9 time-threaded-equihash-pointers.c equihash-pointers-working-minimal-memory-currently-fastest.c --pedantic -Ofast -Wall -std=c11 -o test-equihash -lsodium -lpthread -O3 -march=native -mtune=native -mavx -ftree-vectorize
// theoretical min: 2*2^20*(6*(8+32)
// 2 buckets * NUM_BUCKETS*(BUCKET_SIZE + NUM_ELEMENTS*(ELEMENT_SIZE))

/*
    NOTE: only works for NUM_COLLISION_BITS < 24!
*/

#define EQUIHASH_N 200
#define EQUIHASH_K 9

#define NUM_COLLISION_BITS (EQUIHASH_N / (EQUIHASH_K + 1))
#define NUM_INDICES (1 << EQUIHASH_K)

#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define NUM_BUCKETS (1 << NUM_COLLISION_BITS)
#define DIGEST_SIZE 25

typedef struct element element_t;
typedef uint64_t digest_t[(DIGEST_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];



struct element {
    uint32_t digest_index;
    uint32_t parent_bucket_index;
};


typedef struct bucket {
    unsigned size;
    element_t data[18];
} bucket_t;


void hexout(unsigned char* digest_result) {
    for(unsigned i = 0; i < 4; ++i) {
        for(int j = 0; j < 8; ++j) {
            int c = digest_result[i*8 + j];
            printf("%2X", c);
        }
    }
    printf("\n");
}


uint32_t mask_collision_bits(uint8_t* data, size_t bit_index) {
    uint32_t n = ((*data << (bit_index)) & 0xff) << 12;
    n |= ((*(++data)) << (bit_index+4));
    n |= ((*(++data)) >> (4-bit_index));
    return n;
}


int compare_indices32(uint32_t* a, uint32_t* b, size_t n_current_indices) {
    for(size_t i = 0; i < n_current_indices; ++i, ++a, ++b) {
        if(*a < *b) {
            return -1;
        } else if(*a > *b) {
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

void normalize_indices(uint32_t* indices) {
    for(size_t step_index = 0; step_index < EQUIHASH_K; ++step_index) {
        for(size_t i = 0; i < NUM_INDICES; i += (1 << (step_index+1))) {
            if(compare_indices32(indices+i, indices+i+(1 << step_index), (1 << step_index)) > 0) {
                uint32_t tmp_indices[(1 << step_index)];
                memcpy(tmp_indices, indices+i, (1 << step_index)*sizeof(uint32_t));
                memcpy(indices+i, indices+i+(1 << step_index), (1 << step_index)*sizeof(uint32_t));
                memcpy(indices+i+(1 << step_index), tmp_indices, (1 << step_index)*sizeof(uint32_t));
            }
        }
    }
}


void xor_elements(uint8_t* dst, uint8_t* a, uint8_t* b) {
    ((uint64_t*)dst)[0] = ((uint64_t*)a)[0] ^ ((uint64_t*)b)[0];
    ((uint64_t*)dst)[1] = ((uint64_t*)a)[1] ^ ((uint64_t*)b)[1];
    ((uint64_t*)dst)[2] = ((uint64_t*)a)[2] ^ ((uint64_t*)b)[2];
    dst[24] = a[24] ^ b[24];
}

void hash(uint8_t* dst, uint32_t in, const crypto_generichash_blake2b_state* digest) {
    uint32_t tmp_in = in/2;
    crypto_generichash_blake2b_state new_digest = *digest;
    crypto_generichash_blake2b_update(&new_digest, (uint8_t*)&tmp_in, sizeof(uint32_t));
    crypto_generichash_blake2b_final(&new_digest, (uint8_t*)dst, 2*DIGEST_SIZE);
}


int is_indices_valid(uint32_t* indices, const crypto_generichash_blake2b_state* digest) {
    uint8_t digest_results[NUM_INDICES][DIGEST_SIZE];
    memset(digest_results, '\0', NUM_INDICES*DIGEST_SIZE);

    for(size_t i = 0; i < NUM_INDICES; ++i) {
        uint8_t digest_tmp[2*DIGEST_SIZE];
        hash(digest_tmp, indices[i], digest);
        memcpy(digest_results[i], digest_tmp+((indices[i] % 2)*EQUIHASH_N/8), DIGEST_SIZE);
    }

    for(size_t step_index = 0; step_index < EQUIHASH_K; ++step_index) {
        for(size_t i = 0; i < (NUM_INDICES >> step_index); i += 2) {
            uint8_t digest_tmp[DIGEST_SIZE];
            xor_elements(digest_tmp, digest_results[i], digest_results[i+1]);

            size_t start_bit = step_index*NUM_COLLISION_BITS;
            size_t byte_index = start_bit / 8;
            size_t bit_index = start_bit % 8;

            if(!mask_collision_bits(((uint8_t*)digest_tmp) + byte_index, bit_index) == 0) {
                return 0;
            }

            memcpy(digest_results[i / 2], digest_tmp, DIGEST_SIZE);
        }
    }

    size_t start_bit = EQUIHASH_K*NUM_COLLISION_BITS;
    size_t byte_index = start_bit / 8;
    size_t bit_index = start_bit % 8;
    return mask_collision_bits(((uint8_t*)digest_results[0]) + byte_index, bit_index) == 0;
}


void build(cl_device_id device_id, cl_program program) {
    const char* options = "";
    cl_int ret_val = clBuildProgram(program, 1, &device_id, options, NULL, NULL);

    // avoid abortion due to CL_BILD_PROGRAM_FAILURE

    cl_build_status build_status;
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &build_status, NULL);

    char *build_log;
    size_t ret_val_size;

    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

    build_log = calloc(10000, 1);

    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);

    build_log[ret_val_size] = '\0';
    //fprintf(stderr, "%s\n\n", build_log);
}

const char *get_error_string(cl_int error)
{
switch(error){
    // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    case -9999: return "NVIDIA: ILLEGAL READ OR WRITE TO A BUFFER";
    default:
        fprintf(stderr, "'%d'\n", error);
        return "Unknown OpenCL error";
    }
}


void check_error(cl_int ret, unsigned line_number) {
    if(ret != 0) {
        fprintf(stderr, "An error occured on line %u: %s\n", line_number, get_error_string(ret));
        exit(1);
    }
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


    cl_kernel initial_bucket_hashing_kernel;

    cl_kernel bucket_collide_and_hash_kernel;

    cl_kernel produce_solutions_kernel;



    // gpu variables below
    cl_mem digests[2];
    cl_mem new_digest_index;
    cl_mem buckets;
    cl_mem blake2b_digest;
    cl_mem n_solutions;
    cl_mem dst_solutions;
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

    check_error(clGetPlatformIDs(1, &config->platform_ids, &config->n_platforms), __LINE__);
    check_error(clGetDeviceIDs(config->platform_ids, CL_DEVICE_TYPE_DEFAULT, 1, &config->device_ids, &config->n_devices), __LINE__);
    config->context = clCreateContext(NULL, 1, &config->device_ids, NULL, NULL, &ret);
    check_error(ret, __LINE__);

    config->program = clCreateProgramWithSource(config->context, 1, &config->program_source_code, &config->program_source_code_size, &ret);
    check_error(ret, __LINE__);

    cl_build_status status;
    cl_int err;
    cl_uint platformCount;
    cl_uint deviceCount;
    cl_int r;
    size_t logSize;
    char *programLog;

    if (clBuildProgram(config->program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS){
        // check build log
        clGetProgramBuildInfo(config->program, config->device_ids,
                              CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        programLog = (char*) calloc (logSize+1, sizeof(char));
        clGetProgramBuildInfo(config->program, config->device_ids,
                              CL_PROGRAM_BUILD_LOG, logSize+1, programLog, &r);
        printf("Build failed; error=%d, status=%d, programLog:%s, r: %d\n",
               err, status, programLog, r);
        free(programLog);

    }

    //check_error(clBuildProgram(config->program, 1, &config->device_ids, NULL, NULL, NULL), __LINE__);
    
    config->initial_bucket_hashing_kernel = clCreateKernel(config->program, "initial_bucket_hashing", &ret);
    check_error(ret, __LINE__);

    config->bucket_collide_and_hash_kernel = clCreateKernel(config->program, "bucket_collide_and_hash", &ret);
    check_error(ret, __LINE__);

    config->produce_solutions_kernel = clCreateKernel(config->program, "produce_solutions", &ret);
    check_error(ret, __LINE__);



    config->command_queue = clCreateCommandQueue(config->context, config->device_ids, CL_QUEUE_PROFILING_ENABLE, &ret);
    check_error(ret, __LINE__);

    config->buckets = clCreateBuffer(config->context, CL_MEM_READ_WRITE, NUM_BUCKETS * sizeof(bucket_t) * EQUIHASH_K, NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->buckets, &zero, 1, 0, NUM_BUCKETS * sizeof(bucket_t) * EQUIHASH_K, 0, NULL, NULL), __LINE__);

    config->digests[0] = clCreateBuffer(config->context, CL_MEM_READ_WRITE, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), NULL, &ret);
    check_error(ret, __LINE__);    
    check_error(clEnqueueFillBuffer(config->command_queue, config->digests[0], &zero, 1, 0, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), 0, NULL, NULL), __LINE__);

    config->digests[1] = clCreateBuffer(config->context, CL_MEM_READ_WRITE, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->digests[1], &zero, 1, 0, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), 0, NULL, NULL), __LINE__);

    config->new_digest_index = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->new_digest_index, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL), __LINE__);


    config->blake2b_digest = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(crypto_generichash_blake2b_state), NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->blake2b_digest, &zero, 1, 0, sizeof(crypto_generichash_blake2b_state), 0, NULL, NULL), __LINE__);

    config->dst_solutions = clCreateBuffer(config->context, CL_MEM_READ_WRITE, 20*NUM_INDICES*sizeof(uint32_t), NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->dst_solutions, &zero, 1, 0, 20*NUM_INDICES*sizeof(uint32_t), 0, NULL, NULL), __LINE__);


    config->n_solutions = clCreateBuffer(config->context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    check_error(ret, __LINE__);
    check_error(clEnqueueFillBuffer(config->command_queue, config->n_solutions, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL), __LINE__);

    //fprintf(stderr, "Total gpu buffer %u\n", NUM_BUCKETS * sizeof(bucket_t) * EQUIHASH_K + 2* (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t) + sizeof(crypto_generichash_blake2b_state) + 20*NUM_INDICES*sizeof(uint32_t) + sizeof(uint32_t));

    free(config->program_source_code);
    
}

void cleanup_program(gpu_config_t* config) {
    check_error(clReleaseProgram(config->program), __LINE__);
    check_error(clReleaseKernel(config->initial_bucket_hashing_kernel), __LINE__);
    check_error(clReleaseKernel(config->bucket_collide_and_hash_kernel), __LINE__);
    check_error(clReleaseKernel(config->produce_solutions_kernel), __LINE__);
    check_error(clReleaseCommandQueue(config->command_queue), __LINE__);


    check_error(clReleaseMemObject(config->dst_solutions), __LINE__);
    check_error(clReleaseMemObject(config->n_solutions), __LINE__);
    check_error(clReleaseMemObject(config->digests[0]), __LINE__);
    check_error(clReleaseMemObject(config->digests[1]), __LINE__);
    check_error(clReleaseMemObject(config->buckets), __LINE__);
    check_error(clReleaseMemObject(config->blake2b_digest), __LINE__);
    check_error(clReleaseContext(config->context), __LINE__);
}


size_t equihash(uint32_t* dst_solutions, crypto_generichash_blake2b_state* digest) {
    size_t global_work_offset = 0;
    size_t global_work_size = 1 << 17;
    size_t local_work_size = 32;
    gpu_config_t config;
    init_program(&config, "./equihash.cl", 0);
    
    cl_ulong time_start;
    cl_ulong time_end;
    cl_ulong total_time = 0;
    cl_event timing_events[10];
    cl_int zero = 0;

    check_error(clEnqueueWriteBuffer(config.command_queue, config.blake2b_digest, CL_TRUE, 0, sizeof(crypto_generichash_blake2b_state), (void*)digest, 0, NULL, NULL), __LINE__);


    check_error(clEnqueueFillBuffer(config.command_queue, config.new_digest_index, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL), __LINE__);
    check_error(clSetKernelArg(config.initial_bucket_hashing_kernel, 0, sizeof(cl_mem), (void *)&config.buckets), __LINE__);
    check_error(clSetKernelArg(config.initial_bucket_hashing_kernel, 1, sizeof(cl_mem), (void *)&config.digests[0]), __LINE__);
    check_error(clSetKernelArg(config.initial_bucket_hashing_kernel, 2, sizeof(cl_mem), (void *)&config.blake2b_digest), __LINE__);
    check_error(clSetKernelArg(config.initial_bucket_hashing_kernel, 3, sizeof(cl_mem), (void *)&config.new_digest_index), __LINE__);
    check_error(clEnqueueNDRangeKernel(config.command_queue, config.initial_bucket_hashing_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[0]), __LINE__);
    check_error(clWaitForEvents(1, &timing_events[0]), __LINE__);
    check_error(clGetEventProfilingInfo(timing_events[0], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL), __LINE__);
    check_error(clGetEventProfilingInfo(timing_events[0], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL), __LINE__);
    fprintf(stderr, "step0: %0.3f ms\n", (time_end - time_start) / 1000000.0);
    total_time += (time_end-time_start);


    uint32_t i = 1;
    for(i = 1; i < EQUIHASH_K; ++i) {
        fprintf(stderr, "step: %u\n", i);
        /* Set OpenCL Kernel Parameters */
        check_error(clEnqueueFillBuffer(config.command_queue, config.new_digest_index, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL), __LINE__);

        check_error(clSetKernelArg(config.bucket_collide_and_hash_kernel, 0, sizeof(cl_mem), (void *)&config.digests[i%2]), __LINE__);
        check_error(clSetKernelArg(config.bucket_collide_and_hash_kernel, 1, sizeof(cl_mem), (void *)&config.digests[(i-1)%2]), __LINE__);
        check_error(clSetKernelArg(config.bucket_collide_and_hash_kernel, 2, sizeof(cl_mem), (void *)&config.buckets), __LINE__);
        check_error(clSetKernelArg(config.bucket_collide_and_hash_kernel, 3, sizeof(uint32_t), (void*)&i), __LINE__);
        check_error(clSetKernelArg(config.bucket_collide_and_hash_kernel, 4, sizeof(cl_mem), (void *)&config.new_digest_index), __LINE__);
        check_error(clEnqueueNDRangeKernel(config.command_queue, config.bucket_collide_and_hash_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[i]), __LINE__);
        check_error(clWaitForEvents(1, &timing_events[i]), __LINE__);
        check_error(clGetEventProfilingInfo(timing_events[i], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL), __LINE__);
        check_error(clGetEventProfilingInfo(timing_events[i], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL), __LINE__);
        fprintf(stderr, "step0: %0.3f ms\n", (time_end - time_start) / 1000000.0);
        total_time += (time_end-time_start);

    }

    uint32_t n_solutions = 0;


    check_error(clEnqueueFillBuffer(config.command_queue, config.n_solutions, &zero, 1, 0, sizeof(uint32_t), 0, NULL, NULL), __LINE__);
    check_error(clSetKernelArg(config.produce_solutions_kernel, 0, sizeof(cl_mem), (void *)&config.dst_solutions), __LINE__);
    check_error(clSetKernelArg(config.produce_solutions_kernel, 1, sizeof(cl_mem), (void *)&config.n_solutions), __LINE__);
    check_error(clSetKernelArg(config.produce_solutions_kernel, 2, sizeof(cl_mem), (void *)&config.buckets), __LINE__);
    check_error(clSetKernelArg(config.produce_solutions_kernel, 3, sizeof(cl_mem), (void *)&config.digests[0]), __LINE__);
    check_error(clSetKernelArg(config.produce_solutions_kernel, 4, sizeof(cl_mem), (void *)&config.blake2b_digest), __LINE__);
    check_error(clEnqueueNDRangeKernel(config.command_queue, config.produce_solutions_kernel, 1, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &timing_events[9]), __LINE__);
    check_error(clWaitForEvents(1, &timing_events[9]), __LINE__);
    check_error(clGetEventProfilingInfo(timing_events[9], CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL), __LINE__);
    check_error(clGetEventProfilingInfo(timing_events[9], CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL), __LINE__);
    fprintf(stderr, "step0: %0.3f ms\n", (time_end - time_start) / 1000000.0);
    total_time += (time_end-time_start);

    check_error(clEnqueueReadBuffer(config.command_queue, config.dst_solutions, CL_TRUE, 0, 10*NUM_INDICES*sizeof(uint32_t), dst_solutions, 0, NULL, NULL), __LINE__);
    check_error(clEnqueueReadBuffer(config.command_queue, config.n_solutions, CL_TRUE, 0, sizeof(uint32_t), &n_solutions, 0, NULL, NULL), __LINE__);

    fprintf(stderr, "found %u solutions in %0.3f ms\n", n_solutions, total_time / 1000000.0);

    for(i = 0; i < n_solutions; ++i) {
        normalize_indices(dst_solutions + (NUM_INDICES*i));
    }

    cleanup_program(&config);
    return n_solutions;
}