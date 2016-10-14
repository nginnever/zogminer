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

typedef uint32_t eh_index;typedef uint32_t eh_index;

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
		unsigned _platformId = 0,
		unsigned _deviceId = 0,
		const char* _kernel = "gen_list"
	);

	void run(ulong& _headerIn);

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
	cl::Kernel m_zogKernel;
	cl::Buffer m_base_state;
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
