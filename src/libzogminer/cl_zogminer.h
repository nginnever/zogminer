#pragma once

#define __CL_ENABLE_EXCEPTIONS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

// Just for Hello World Kernel
#define DATA_SIZE 100

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "cl.hpp"
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-protector"
#include "cl.hpp"
#pragma GCC diagnostic pop
#else
#include "cl.hpp"
#endif
#include <time.h>
#include <functional>

#include "sodium.h"

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

typedef uint32_t eh_index;

class cl_zogminer
{

public:

	cl_zogminer();
	~cl_zogminer();

	static bool searchForAllDevices(unsigned _platformId, std::function<bool(cl::Device const&)> _callback);
	static bool searchForAllDevices(std::function<bool(cl::Device const&)> _callback);
	static void doForAllDevices(unsigned _platformId, std::function<void(cl::Device const&)> _callback);
	static void doForAllDevices(std::function<void(cl::Device const&)> _callback);
	static unsigned getNumPlatforms();
	static unsigned getNumDevices(unsigned _platformId = 0);
	static std::string platform_info(unsigned _platformId = 0, unsigned _deviceId = 0);
	static void listDevices();

	// Currently just prints memory of the GPU
	static bool configureGPU(
		unsigned _platformId,
		unsigned _localWorkSize,
		unsigned _globalWorkSize
	);

	bool init(
		unsigned _platformId,
		unsigned _deviceId,
		std::vector<std::string> _kernels
	);

	void run(crypto_generichash_blake2b_state base_state, uint32_t * sols, uint32_t * n_sol);

	void finish();

	/* -- default values -- */
	/// Default value of the local work size. Also known as workgroup size.
	static unsigned const c_defaultLocalWorkSize;
	/// Default value of the global work size as a multiplier of the local work size
	static unsigned const c_defaultGlobalWorkSizeMultiplier;
	/// Default value of the milliseconds per global work size (per batch)
	static unsigned const c_defaultMSPerBatch;

private:
  static const unsigned int z_n = 200;
  static const unsigned int z_k = 9;
  static const size_t z_collision_bit_length = z_n / (z_k + 1);
  static const eh_index z_N = 1 << (z_collision_bit_length + 1);

	static std::vector<cl::Device> getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId);
	static std::vector<cl::Platform> getPlatforms();
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
	cl::Context m_context;
	cl::CommandQueue m_queue;
	std::vector<cl::Kernel> m_zogKernels;
	cl::Buffer m_digests[2];
	cl::Buffer m_buckets;
	cl::Buffer m_new_digest_index;
	cl::Buffer m_blake2b_digest;
	cl::Buffer m_dst_solutions;
	cl::Buffer m_n_solutions;
	const cl_int zero = 0;
	uint32_t * solutions;
	uint32_t * dst_solutions;
	
	unsigned m_globalWorkSize;
	bool m_openclOnePointOne;
	unsigned m_deviceBits;

	/// The step used in the work size adjustment
	unsigned int m_stepWorkSizeAdjust;
	/// The Work Size way of adjustment, > 0 when previously increased, < 0 when previously decreased
	int m_wayWorkSizeAdjust = 0;

	/// The local work size for the search
	static unsigned s_workgroupSize;
	/// The initial global work size for the searches
	static unsigned s_initialGlobalWorkSize;
	/// The target milliseconds per batch for the search. If 0, then no adjustment will happen
	static unsigned s_msPerBatch;
	/// Allow CPU to appear as an OpenCL device or not. Default is false
	static bool s_allowCPU;
	/// GPU memory required for other things, like window rendering e.t.c.
	/// User can set it via the --cl-extragpu-mem argument.
	static unsigned s_extraRequiredGPUMem;
};
