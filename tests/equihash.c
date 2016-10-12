/* MIT License
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define DATA_SIZE 10
#define LIST_SIZE 2097152
#define FULL_WIDTH 1030
// The maximum size of the .cl file we read in and compile
#define MAX_SOURCE_SIZE     (0x200000)

typedef uint32_t eh_index;

int main(void)
{
    cl_context context;
    cl_context_properties properties[3];
    cl_kernel kernel;
    cl_kernel kernel2;
    cl_command_queue command_queue;
    cl_program program;
    cl_int err;
    cl_uint num_of_platforms=0;
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_uint num_of_devices=0;
    cl_mem base_state;
    cl_mem list;
    cl_ulong max_bytes;
    cl_int wait_status;
    cl_event event;
    
    size_t global;
    
    unsigned long inputDataA[DATA_SIZE]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    unsigned char list_element[FULL_WIDTH];
    //float inputDataB[DATA_SIZE]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    //float results[DATA_SIZE]={0};
    
    unsigned long n = 200;
    unsigned long k = 9;
    
    size_t collision_bit_length = n / (k + 1);
    eh_index init_size = 1 << (collision_bit_length + 1);
    //unsigned long indices_per_hash_out = 512 / n;
    //size_t collision_byte_length = (collision_bit_length + 7) / 8;
    //size_t hash_len = (k + 1) * collision_byte_length;
    //size_t len_indices = sizeof(eh_index);
    //size_t full_width = 2 * collision_byte_length + sizeof(eh_index) * (1 << (k-1));
    //const size_t hash_output = indices_per_hash_out * n / 8; //Kernel arg
    
    //printf("size_t: %i vs. %i\n",sizeof(size_t),sizeof(unsigned int));
    
    //TODO We need to create this state with Block info, now it is empty
    //eh_hash_state base_state;
    printf("Init Size: %i\n", init_size);
    
    // retreive a list of platforms avaible
    if (clGetPlatformIDs(1, &platform_id, &num_of_platforms)!= CL_SUCCESS)
    {
        printf("Unable to get platform_id\n");
        return 1;
    }
    
    // try to get a supported GPU device
    if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_of_devices) != CL_SUCCESS)
    {
        printf("Unable to get device_id\n");
        return 1;
    }
    
    size_t rett;
    clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(max_bytes), &max_bytes, &rett);
    printf("MAX GPU MEM BYTES %u\n", max_bytes);
    printf("MAX GPU MEM ALLOCATION %u\n", max_bytes/4);
    
    // context properties list - must be terminated with 0
    properties[0]= CL_CONTEXT_PLATFORM;
    properties[1]= (cl_context_properties) platform_id;
    properties[2]= 0;
    
    // create a context with the GPU device
    context = clCreateContext(properties,1,&device_id,NULL,NULL,&err);
    
    // create command queue using the context and device
    command_queue = clCreateCommandQueue(context, device_id, 0, &err);
    
    // Load kernel source file.
    printf("Initializing...\n");
    fflush(stdout);
    FILE *fp;
    const char fileName[] = "./list-gen.cl";
    size_t source_size;
    char *source_str;
    fp = fopen(fileName, "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = (char *)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
    
    cl_build_status status;
    size_t logSize;
    char *programLog;
    // create a program from the kernel source code
    program = clCreateProgramWithSource(context, 1, (const char **) &source_str, (const size_t *)&source_size, &err);
    // compile the program
    if (clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
    {
        // check build error and build status first
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS,
                              sizeof(cl_build_status), &status, NULL);
        
        // check build log
        clGetProgramBuildInfo(program, device_id,
                              CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        programLog = (char*) calloc (logSize+1, sizeof(char));
        clGetProgramBuildInfo(program, device_id,
                              CL_PROGRAM_BUILD_LOG, logSize+1, programLog, NULL);
        printf("Build failed; error=%d, status=%d, programLog:nn%s",
               err, status, programLog);
        free(programLog);
        
        return 1;
    }
    
    // specify which kernel from the program to execute
    kernel = clCreateKernel(program, "list_gen", &err);
    kernel2 = clCreateKernel(program, "test", &err);
    
    // create buffers for the input and ouput
    
    base_state = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned long) * DATA_SIZE, NULL, NULL);
    list = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * 129 * LIST_SIZE, NULL, NULL);
    //output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * DATA_SIZE, NULL, NULL);
    printf("global buffer size in bytes: %u\n", sizeof(unsigned char) * 129 * LIST_SIZE);
    
    // load data into the input buffer
    clEnqueueWriteBuffer(command_queue, base_state, CL_TRUE, 0, sizeof(unsigned long) * DATA_SIZE, inputDataA, 0, NULL, NULL);
    // create global memory buffer to store the list + indices in
    // size = length of list (2097152) * sizeof uchar (1 byte) * sizeof full_length of hash+indices (1030 bytes)
    //clEnqueueWriteBuffer(command_queue, list, CL_TRUE, 0, sizeof(unsigned char) * FULL_WIDTH * LIST_SIZE, list_element, 0, NULL, NULL);
    
    // set the argument list for the kernel command
    //TODO check size of hash_output, size_t can differ
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &base_state);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &list);
    clSetKernelArg(kernel2, 0, sizeof(cl_mem), &list);
    
    global=init_size;
    size_t g2 = 2000;
    //global=DATA_SIZE;
    
    printf("Starting list generation in GPU...\n");
    
    // enqueue the kernel command for execution
    clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global, NULL, 0, NULL, &event);
    wait_status = clWaitForEvents(1, &event);
    printf("wait status %i\n", wait_status);
    //clReleaseEvent(event);
    //clFinish(command_queue);
    //clReleaseKernel(kernel);

    clEnqueueNDRangeKernel(command_queue, kernel2, 1, NULL, &g2, NULL, 0, NULL, NULL);
    clFinish(command_queue);
    
    printf("List generation in GPU done...\n");

    
    // copy the results from out of the output buffer
    /*clEnqueueReadBuffer(command_queue, output, CL_TRUE, 0, sizeof(float) *DATA_SIZE, results, 0, NULL, NULL);*/
    
    // print the results
    /*printf("output: ");
    
    int i;
    for(i=0;i<DATA_SIZE; i++)
    {
        printf("%f ",results[i]);
    }
    printf("\n");*/
    
    // cleanup - release OpenCL resources
    clReleaseMemObject(base_state);
    /*clReleaseMemObject(inputB);
    clReleaseMemObject(output);*/
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);
    
    return 0;
    
}
