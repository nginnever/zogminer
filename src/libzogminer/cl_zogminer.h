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

#define NUM_COMPRESSED_INDICE_BITS 16
#define NUM_DECOMPRESSED_INDICE_BITS (NUM_COLLISION_BITS+1)

#define NUM_INDICE_BYTES_PER_ELEMENT (((NUM_INDICES/2) * NUM_COMPRESSED_INDICE_BITS + 7) / 8)
#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define NUM_INDICES_PER_BUCKET (1 << 10)
#define NUM_STEP_INDICES (8*NUM_VALUES)
#define NUM_BUCKETS (1 << NUM_COLLISION_BITS)/NUM_INDICES_PER_BUCKET
#define DIGEST_SIZE 32

typedef struct element_indice {
    uint32_t a;
    uint32_t b;
} element_indice_t;

typedef struct element {
    uint8_t digest[DIGEST_SIZE];
    uint32_t a;
    uint32_t b;
} element_t;

typedef struct bucket {
    uint32_t tmp;
    uint32_t size;
    element_t data[NUM_INDICES_PER_BUCKET*4];
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

	void run(crypto_generichash_blake2b_state base_state);

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
	cl::Context m_context;
	cl::CommandQueue m_queue;
	std::vector<cl::Kernel> m_zogKernels;
	cl::Buffer m_indices;
	cl::Buffer m_src_bucket;
	cl::Buffer m_dst_bucket;
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
