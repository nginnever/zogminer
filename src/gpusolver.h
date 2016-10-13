#ifndef __GPU_SOLVER_H
#define __GPU_SOLVER_H

#include <cstdio>
#include <csignal>
#include <iostream>

#include "crypto/equihash.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

// The maximum size of the .cl file we read in and compile
#define MAX_SOURCE_SIZE 	(0x200000)

class GPUSolverCancelledException : public std::exception
{
    virtual const char* what() const throw() {
        return "GPU Equihash solver was cancelled";
    }
};

enum GPUSolverCancelCheck
{
    ListGenerationGPU,
    ListSortingGPU
};

class GPUSolver {
	
private:
	//TODO Really wasteful initialize and compile kernels once, not for every iter
	bool GPUSolve200_9(const eh_HashState& base_state,
		         	const std::function<bool(std::vector<unsigned char>)> validBlock,
				const std::function<bool(GPUSolverCancelCheck)> cancelled);

public:
	GPUSolver();
        bool run(unsigned int n, unsigned int k, const eh_HashState& base_state,
		            const std::function<bool(std::vector<unsigned char>)> validBlock,
				const std::function<bool(GPUSolverCancelCheck)> cancelled);

};

#endif // __GPU_SOLVER_H
