
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstdlib>
//#include <chrono>
#include <fstream>
#include <iostream>
#include <assert.h>
//#include <queue>
#include <vector>
#include <random>
//#include <atomic>
//#include <sstream>
#include "cl_zogminer.h"
#include "cl_zogminer_kernel.h" // Created from CMake

// workaround lame platforms
#if !CL_VERSION_1_2
#define CL_MAP_WRITE_INVALIDATE_REGION CL_MAP_WRITE
#define CL_MEM_HOST_READ_ONLY 0
#endif

#undef min
#undef max

using namespace std;

unsigned const cl_zogminer::c_defaultLocalWorkSize = 64;
unsigned const cl_zogminer::c_defaultGlobalWorkSizeMultiplier = 4096; // * CL_DEFAULT_LOCAL_WORK_SIZE
unsigned const cl_zogminer::c_defaultMSPerBatch = 0;
bool cl_zogminer::s_allowCPU = false;
unsigned cl_zogminer::s_extraRequiredGPUMem;
unsigned cl_zogminer::s_msPerBatch = cl_zogminer::c_defaultMSPerBatch;
unsigned cl_zogminer::s_workgroupSize = cl_zogminer::c_defaultLocalWorkSize;
unsigned cl_zogminer::s_initialGlobalWorkSize = cl_zogminer::c_defaultGlobalWorkSizeMultiplier * cl_zogminer::c_defaultLocalWorkSize;

#if defined(_WIN32)
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
static std::atomic_flag s_logSpin = ATOMIC_FLAG_INIT;
#define CL_LOG(_contents) \
	do \
	{ \
		std::stringstream ss; \
		ss << _contents; \
		while (s_logSpin.test_and_set(std::memory_order_acquire)) {} \
		OutputDebugStringA(ss.str().c_str()); \
		cerr << ss.str() << endl << flush; \
		s_logSpin.clear(std::memory_order_release); \
	} while (false)
#else
#define CL_LOG(_contents) cout << "[OPENCL]:" << _contents << endl
#endif

// Types of OpenCL devices we are interested in
#define CL_QUERIED_DEVICE_TYPES (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR)

// Inject definitions into the kernal source
static void addDefinition(string& _source, char const* _id, unsigned _value)
{
	char buf[256];
	sprintf(buf, "#define %s %uu\n", _id, _value);
	_source.insert(_source.begin(), buf, buf + strlen(buf));
}


cl_zogminer::cl_zogminer()
:	m_openclOnePointOne()
{
}

cl_zogminer::~cl_zogminer()
{
	finish();
}

std::vector<cl::Platform> cl_zogminer::getPlatforms()
{
	vector<cl::Platform> platforms;
	try
	{
		cl::Platform::get(&platforms);
	}
	catch(cl::Error const& err)
	{
#if defined(CL_PLATFORM_NOT_FOUND_KHR)
		if (err.err() == CL_PLATFORM_NOT_FOUND_KHR)
			CL_LOG("No OpenCL platforms found");
		else
#endif
			throw err;
	}
	return platforms;
}

string cl_zogminer::platform_info(unsigned _platformId, unsigned _deviceId)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return {};
	// get GPU device of the selected platform
	unsigned platform_num = min<unsigned>(_platformId, platforms.size() - 1);
	vector<cl::Device> devices = getDevices(platforms, _platformId);
	if (devices.empty())
	{
		CL_LOG("No OpenCL devices found.");
		return {};
	}

	// use selected default device
	unsigned device_num = min<unsigned>(_deviceId, devices.size() - 1);
	cl::Device& device = devices[device_num];
	string device_version = device.getInfo<CL_DEVICE_VERSION>();

	return "{ \"platform\": \"" + platforms[platform_num].getInfo<CL_PLATFORM_NAME>() + "\", \"device\": \"" + device.getInfo<CL_DEVICE_NAME>() + "\", \"version\": \"" + device_version + "\" }";
}

std::vector<cl::Device> cl_zogminer::getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId)
{
	vector<cl::Device> devices;
	unsigned platform_num = min<unsigned>(_platformId, _platforms.size() - 1);
	try
	{
		_platforms[platform_num].getDevices(
			s_allowCPU ? CL_DEVICE_TYPE_ALL : CL_QUERIED_DEVICE_TYPES,
			&devices
		);
	}
	catch (cl::Error const& err)
	{
		// if simply no devices found return empty vector
		if (err.err() != CL_DEVICE_NOT_FOUND)
			throw err;
	}
	return devices;
}

unsigned cl_zogminer::getNumPlatforms()
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return 0;
	return platforms.size();
}

unsigned cl_zogminer::getNumDevices(unsigned _platformId)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return 0;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	if (devices.empty())
	{
		CL_LOG("No OpenCL devices found.");
		return 0;
	}
	return devices.size();
}

// This needs customizing apon completion of the kernel - Checks memory requirements - May not be applicable
bool cl_zogminer::configureGPU(
	unsigned _platformId
	//unsigned _localWorkSize,
	//unsigned _globalWorkSize,
	//unsigned _msPerBatch,
	//bool _allowCPU,
	//unsigned _extraGPUMemory
	// uint64_t _currentBlock
)
{
	//s_workgroupSize = _localWorkSize;
	//s_initialGlobalWorkSize = _globalWorkSize;
	//s_msPerBatch = _msPerBatch;
	//s_allowCPU = _allowCPU;
	//s_extraRequiredGPUMem = _extraGPUMemory;

	return searchForAllDevices(_platformId, [](cl::Device const& _device) -> bool
		{
			cl_ulong result;
			_device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);

			//if (result >= requiredSize)
			//{
				CL_LOG(
					"Found suitable OpenCL device [" << _device.getInfo<CL_DEVICE_NAME>()
					<< "] with " << result << " bytes of GPU memory"
				);
				return true;
			//}
			/*
			CL_LOG(
				"OpenCL device " << _device.getInfo<CL_DEVICE_NAME>()
				<< " has insufficient GPU memory." << result <<
				" bytes of memory found < " << requiredSize << " bytes of memory required"
			);
			return false;
		*/
		}
	);
}

bool cl_zogminer::searchForAllDevices(function<bool(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return false;
	for (unsigned i = 0; i < platforms.size(); ++i)
		if (searchForAllDevices(i, _callback))
			return true;

	return false;
}

bool cl_zogminer::searchForAllDevices(unsigned _platformId, function<bool(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return false;
	if (_platformId >= platforms.size())
		return false;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	for (cl::Device const& device: devices)
		if (_callback(device))
			return true;

	return false;
}

void cl_zogminer::doForAllDevices(function<void(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return;
	for (unsigned i = 0; i < platforms.size(); ++i)
		doForAllDevices(i, _callback);
}

void cl_zogminer::doForAllDevices(unsigned _platformId, function<void(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return;
	if (_platformId >= platforms.size())
		return;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	for (cl::Device const& device: devices)
		_callback(device);
}

void cl_zogminer::listDevices()
{
	string outString ="\nListing OpenCL devices.\nFORMAT: [deviceID] deviceName\n";
	unsigned int i = 0;
	doForAllDevices([&outString, &i](cl::Device const _device)
		{
			outString += "[" + to_string(i) + "] " + _device.getInfo<CL_DEVICE_NAME>() + "\n";
			outString += "\tCL_DEVICE_TYPE: ";
			switch (_device.getInfo<CL_DEVICE_TYPE>())
			{
			case CL_DEVICE_TYPE_CPU:
				outString += "CPU\n";
				break;
			case CL_DEVICE_TYPE_GPU:
				outString += "GPU\n";
				break;
			case CL_DEVICE_TYPE_ACCELERATOR:
				outString += "ACCELERATOR\n";
				break;
			default:
				outString += "DEFAULT\n";
				break;
			}
			outString += "\tCL_DEVICE_GLOBAL_MEM_SIZE: " + to_string(_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()) + "\n";
			outString += "\tCL_DEVICE_MAX_MEM_ALLOC_SIZE: " + to_string(_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()) + "\n";
			outString += "\tCL_DEVICE_MAX_WORK_GROUP_SIZE: " + to_string(_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
			++i;
		}
	);
	CL_LOG(outString);
}

void cl_zogminer::finish()
{
	if (m_queue())
		m_queue.finish();
}

// Customise given kernel - This builds the kernel and creates memory buffers
bool cl_zogminer::init(
	unsigned _platformId,
	unsigned _deviceId
)
{
	// get all platforms
	try
	{
		vector<cl::Platform> platforms = getPlatforms();
		if (platforms.empty())
			return false;

		// use selected platform
		_platformId = min<unsigned>(_platformId, platforms.size() - 1);
		CL_LOG("Using platform: " << platforms[_platformId].getInfo<CL_PLATFORM_NAME>().c_str());

		// get GPU device of the default platform
		vector<cl::Device> devices = getDevices(platforms, _platformId);
		if (devices.empty())
		{
			CL_LOG("No OpenCL devices found.");
			return false;
		}

		// use selected device
		cl::Device& device = devices[min<unsigned>(_deviceId, devices.size() - 1)];
		string device_version = device.getInfo<CL_DEVICE_VERSION>();
		CL_LOG("Using device: " << device.getInfo<CL_DEVICE_NAME>().c_str() << "(" << device_version.c_str() << ")");

		if (strncmp("OpenCL 1.0", device_version.c_str(), 10) == 0)
		{
			CL_LOG("OpenCL 1.0 is not supported.");
			return false;
		}
		if (strncmp("OpenCL 1.1", device_version.c_str(), 10) == 0)
			m_openclOnePointOne = true;

		// create context
		m_context = cl::Context(vector<cl::Device>(&device, &device + 1));
		m_queue = cl::CommandQueue(m_context, device);

		// make sure that global work size is evenly divisible by the local workgroup size
		m_globalWorkSize = s_initialGlobalWorkSize;
		if (m_globalWorkSize % s_workgroupSize != 0)
			m_globalWorkSize = ((m_globalWorkSize / s_workgroupSize) + 1) * s_workgroupSize;
		// remember the device's address bits
		m_deviceBits = device.getInfo<CL_DEVICE_ADDRESS_BITS>();
		// make sure first step of global work size adjustment is large enough
		m_stepWorkSizeAdjust = pow(2, m_deviceBits / 2 + 1);

		// patch source code
		// note: CL_MINER_KERNEL is simply cl_zogminer_kernel.cl compiled
		// into a byte array by bin2h.cmake. There is no need to load the file by hand in runtime
		string code(CL_MINER_KERNEL, CL_MINER_KERNEL + CL_MINER_KERNEL_SIZE);
		// add required definitions here
		addDefinition(code, "GROUP_SIZE", s_workgroupSize);
		//addDefinition(code, "ACCESSES", ACCESSES);

		// create miner OpenCL program
		cl::Program::Sources sources;
		sources.push_back({ code.c_str(), code.size() });

		cl::Program program(m_context, sources);
		try
		{
			program.build({ device });
			CL_LOG("Printing program log");
			CL_LOG(program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str());
		}
		catch (cl::Error const&)
		{
			CL_LOG(program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str());
			return false;
		}

		// create buffer kernel inputs (private variables)

	  m_input = cl::Buffer(m_context, CL_MEM_READ_ONLY,
	  sizeof(float) * DATA_SIZE, NULL, NULL);
	  m_output = cl::Buffer(m_context, CL_MEM_WRITE_ONLY,
	  sizeof(float) * DATA_SIZE, NULL, NULL);

	}
	catch (cl::Error const& err)
	{
		CL_LOG(err.what() << "(" << err.err() << ")");
		return false;
	}
	return true;
}

/*
void cl_zogminer::search(uint8_t const* header, uint64_t target, search_hook& hook)
{
	try
	{
		struct pending_batch
		{
			uint64_t start_nonce;
			unsigned buf;
		};
		queue<pending_batch> pending;

		// this can't be a static because in MacOSX OpenCL implementation a segfault occurs when a static is passed to OpenCL functions
		uint32_t const c_zero = 0;

		// update header constant buffer
		m_queue.enqueueWriteBuffer(m_header, false, 0, 32, header);
		for (unsigned i = 0; i != c_bufferCount; ++i)
			m_queue.enqueueWriteBuffer(m_searchBuffer[i], false, 0, 4, &c_zero);

#if CL_VERSION_1_2 && 0
		cl::Event pre_return_event;
		if (!m_opencl_1_1)
			m_queue.enqueueBarrierWithWaitList(NULL, &pre_return_event);
		else
#endif
			m_queue.finish();

		unsigned argPos = 2;
		m_searchKernel.setArg(1, m_header);
		for (unsigned i = 0; i < m_dagChunksCount; ++i, ++argPos)
			m_searchKernel.setArg(argPos, m_dagChunks[i]);
		// pass these to stop the compiler unrolling the loops
		m_searchKernel.setArg(argPos + 1, target);
		m_searchKernel.setArg(argPos + 2, ~0u);

		unsigned buf = 0;
		random_device engine;
		uint64_t start_nonce = uniform_int_distribution<uint64_t>()(engine);
		for (;; start_nonce += m_globalWorkSize)
		{
			auto t = chrono::high_resolution_clock::now();
			// supply output buffer to kernel
			m_searchKernel.setArg(0, m_searchBuffer[buf]);
			if (m_dagChunksCount == 1)
				m_searchKernel.setArg(3, start_nonce);
			else
				m_searchKernel.setArg(6, start_nonce);

			// execute it!
			m_queue.enqueueNDRangeKernel(m_searchKernel, cl::NullRange, m_globalWorkSize, s_workgroupSize);

			pending.push({ start_nonce, buf });
			buf = (buf + 1) % c_bufferCount;

			// read results
			if (pending.size() == c_bufferCount)
			{
				pending_batch const& batch = pending.front();

				// could use pinned host pointer instead
				uint32_t* results = (uint32_t*)m_queue.enqueueMapBuffer(m_searchBuffer[batch.buf], true, CL_MAP_READ, 0, (1 + c_maxSearchResults) * sizeof(uint32_t));
				unsigned num_found = min<unsigned>(results[0], c_maxSearchResults);

				uint64_t nonces[c_maxSearchResults];
				for (unsigned i = 0; i != num_found; ++i)
					nonces[i] = batch.start_nonce + results[i + 1];

				m_queue.enqueueUnmapMemObject(m_searchBuffer[batch.buf], results);
				bool exit = num_found && hook.found(nonces, num_found);
				exit |= hook.searched(batch.start_nonce, m_globalWorkSize); // always report searched before exit
				if (exit)
					break;

				// reset search buffer if we're still going
				if (num_found)
					m_queue.enqueueWriteBuffer(m_searchBuffer[batch.buf], true, 0, 4, &c_zero);

				pending.pop();
			}

			// adjust global work size depending on last search time
			if (s_msPerBatch)
			{
				// Global work size must be:
				//  - less than or equal to 2 ^ DEVICE_BITS - 1
				//  - divisible by lobal work size (workgroup size)
				auto d = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - t);
				if (d != chrono::milliseconds(0)) // if duration is zero, we did not get in the actual searh/or search not finished
				{
					if (d > chrono::milliseconds(s_msPerBatch * 10 / 9))
					{
						// Divide the step by 2 when adjustment way change
						if (m_wayWorkSizeAdjust > -1)
							m_stepWorkSizeAdjust = max<unsigned>(1, m_stepWorkSizeAdjust / 2);
						m_wayWorkSizeAdjust = -1;
						// cerr << "m_stepWorkSizeAdjust: " << m_stepWorkSizeAdjust << ", m_wayWorkSizeAdjust: " << m_wayWorkSizeAdjust << endl;

						// cerr << "Batch of " << m_globalWorkSize << " took " << chrono::duration_cast<chrono::milliseconds>(d).count() << " ms, >> " << s_msPerBatch << " ms." << endl;
						m_globalWorkSize = max<unsigned>(128, m_globalWorkSize - m_stepWorkSizeAdjust);
						// cerr << "New global work size" << m_globalWorkSize << endl;
					}
					else if (d < chrono::milliseconds(s_msPerBatch * 9 / 10))
					{
						// Divide the step by 2 when adjustment way change
						if (m_wayWorkSizeAdjust < 1)
							m_stepWorkSizeAdjust = max<unsigned>(1, m_stepWorkSizeAdjust / 2);
						m_wayWorkSizeAdjust = 1;
						// cerr << "m_stepWorkSizeAdjust: " << m_stepWorkSizeAdjust << ", m_wayWorkSizeAdjust: " << m_wayWorkSizeAdjust << endl;

						// cerr << "Batch of " << m_globalWorkSize << " took " << chrono::duration_cast<chrono::milliseconds>(d).count() << " ms, << " << s_msPerBatch << " ms." << endl;
						m_globalWorkSize = min<unsigned>(pow(2, m_deviceBits) - 1, m_globalWorkSize + m_stepWorkSizeAdjust);
						// Global work size should never be less than the workgroup size
						m_globalWorkSize = max<unsigned>(s_workgroupSize,  m_globalWorkSize);
						// cerr << "New global work size" << m_globalWorkSize << endl;
					}
				}
			}
		}

		// not safe to return until this is ready
#if CL_VERSION_1_2 && 0
		if (!m_opencl_1_1)
			pre_return_event.wait();
#endif
	}
	catch (cl::Error const& err)
	{
		CL_LOG(err.what() << "(" << err.err() << ")");
	}
}
*/
