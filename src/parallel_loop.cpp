#define __CL_ENABLE_EXCEPTIONS
#define MAX_SOURCE_SIZE (0x100000)
 
#include <fstream>
#include <iostream>
#include <iterator>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include <stdio.h>
#include <sys/time.h>

void cpu_3d_loop (int x, int y, int z) {
 
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            for (int k = 0; k < z; k++) {
                printf("CPU %d,%d,%d\n", i, j, k);
            }
        }
    }
 
}

int main () {
 
    // CPU 3d loop
 
    int x = 4;
    int y = 3;
    int z = 2;

    struct timeval stop, start;
    gettimeofday(&start, NULL);

    cpu_3d_loop(x, y, z);
    gettimeofday(&stop, NULL);

    printf("CPU Execution took: %lums\n", stop.tv_usec - start.tv_usec);

    cl_device_id device_id = NULL;
    cl_context context = NULL;
    cl_command_queue command_queue = NULL;
    cl_mem memobj = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_platform_id platform_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;
    cl_build_status status;
 
    // GPU 3d loop
     
    FILE *fp;
    char fileName[] = "../lib/kernels/ndrange_parallelism.cl";
    char *source_str;
    size_t source_size;
     
    /* Load the source code containing the kernel*/
    fp = fopen(fileName, "r");
    if (!fp) {
    fprintf(stderr, "Failed to load kernel.\n");
    exit(1);
    }

    source_str = (char*)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
     
    /* Get Platform and Device Info */
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &ret_num_devices);
     
    /* Create OpenCL context */
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
     
    /* Create Command Queue */
    command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
     
     
    /* Create Kernel Program from the source */
    program = clCreateProgramWithSource(context, 1, (const char **)&source_str,
    (const size_t *)&source_size, &ret);
     
    /* Build Kernel Program */
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

  if (ret != CL_SUCCESS)
    {
        size_t len;
        char buffer[204800];
    cl_build_status bldstatus;
    printf("\nError %d: Failed to build program executable [ %s ]\n",ret);
        ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(bldstatus), (void *)&bldstatus, &len);
        if (ret != CL_SUCCESS)
          {
        printf("Build Status error %d: %s\n",ret);
        exit(1);
      }     
    if (bldstatus == CL_BUILD_SUCCESS) printf("Build Status: CL_BUILD_SUCCESS\n");
    if (bldstatus == CL_BUILD_NONE) printf("Build Status: CL_BUILD_NONE\n"); 
    if (bldstatus == CL_BUILD_ERROR) printf("Build Status: CL_BUILD_ERROR\n");
    if (bldstatus == CL_BUILD_IN_PROGRESS) printf("Build Status: CL_BUILD_IN_PROGRESS\n");  
        ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_OPTIONS, sizeof(buffer), buffer, &len);
        if (ret != CL_SUCCESS)
          {
        printf("Build Options error %d: %s\n",ret);
        exit(1);
      }        
    printf("Build Options: %s\n", buffer);  
        ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);    
        if (ret != CL_SUCCESS)
          {
        printf("Build Log error %d: %s\n",ret);
        exit(1);
      }     
    printf("Build Log:\n%s\n", buffer);
    exit(1);
    }


    kernel = clCreateKernel(program, "ndrange_parallelism", &ret);

    size_t global_work_size[3] = { x, y, z };
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 3, NULL, global_work_size, NULL, NULL, NULL, NULL);

    /* Finalization */

    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);
    return 0;
     
}