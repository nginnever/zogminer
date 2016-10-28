
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstdlib>
//#include <chrono>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <queue>
#include <vector>
#include <random>
//#include <atomic>
#include "cl_zogminer.h"
#include "kernels/cl_zogminer_kernel.h" // Created from CMake

// workaround lame platforms
#if !CL_VERSION_1_2
#define CL_MAP_WRITE_INVALIDATE_REGION CL_MAP_WRITE
#define CL_MEM_HOST_READ_ONLY 0
#endif

#undef min
#undef max

#define DEBUG

using namespace std;

unsigned const cl_zogminer::c_defaultLocalWorkSize = 32;
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

	dst_solutions = (uint32_t *) malloc(10*NUM_INDICES*sizeof(uint32_t));
	if(dst_solutions == NULL)
		std::cout << "Error allocating dst_solutions array!" << std::endl; 

}

cl_zogminer::~cl_zogminer()
{
	if(dst_solutions != NULL)
		free(dst_solutions);
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
	unsigned _platformId,
	unsigned _localWorkSize,
	unsigned _globalWorkSize
)
{
	// Set the local/global work sizes
	s_workgroupSize = _localWorkSize;
	s_initialGlobalWorkSize = _globalWorkSize;

	return searchForAllDevices(_platformId, [](cl::Device const& _device) -> bool
		{
			cl_ulong result;
			_device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);

				CL_LOG(
					"Found suitable OpenCL device [" << _device.getInfo<CL_DEVICE_NAME>()
					<< "] with " << result << " bytes of GPU memory"
				);
				return true;
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
	unsigned _deviceId,
	const std::vector<std::string> _kernels
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

		// Uncomment for loading kernel from compiled cl file.
#ifdef DEBUG
		ifstream kernel_file("./libzogminer/kernels/cl_zogminer_kernel.cl");
		string code((istreambuf_iterator<char>(kernel_file)), istreambuf_iterator<char>());
		kernel_file.close();
#else
		string code(CL_MINER_KERNEL, CL_MINER_KERNEL + CL_MINER_KERNEL_SIZE);
#endif
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

		try
		{
			for (auto & _kernel : _kernels)
				m_zogKernels.push_back(cl::Kernel(program, _kernel.c_str()));
		}
		catch (cl::Error const& err)
		{
			CL_LOG("ZOGKERNEL Creation failed: " << err.what() << "(" << err.err() << "). Bailing.");
			return false;
		}

		// TODO create buffer kernel inputs (private variables)
	  	m_digests[0] = cl::Buffer(m_context, CL_MEM_READ_WRITE, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), NULL, NULL);
		m_digests[1] = cl::Buffer(m_context, CL_MEM_READ_WRITE, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), NULL, NULL);

		m_buckets = cl::Buffer(m_context, CL_MEM_READ_WRITE, NUM_BUCKETS * sizeof(bucket_t) * EQUIHASH_K, NULL, NULL);

		m_new_digest_index = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, NULL);

		m_blake2b_digest = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof(crypto_generichash_blake2b_state), NULL, NULL);

		m_dst_solutions = cl::Buffer(m_context, CL_MEM_READ_WRITE, 20*NUM_INDICES*sizeof(uint32_t), NULL, NULL);

		m_n_solutions = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, NULL);

		m_src_local_buckets = cl::Buffer(m_context, CL_MEM_READ_WRITE, NUM_BUCKETS * NUM_INDICES_PER_BUCKET * sizeof(src_local_bucket_t), NULL, NULL);

		m_dst_candidates = cl::Buffer(m_context, CL_MEM_READ_WRITE, (1 << 16)*sizeof(element_t), NULL, NULL);

		m_n_candidates = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, NULL);

		m_elements = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof(element_t)*EQUIHASH_K*NUM_INDICES/2*(1<<13), NULL, NULL);

		m_queue.finish();

	}
	catch (cl::Error const& err)
	{
		CL_LOG("CL ERROR:" << get_error_string(err.err()));
		return false;
	}
	return true;
}


void cl_zogminer::run(crypto_generichash_blake2b_state base_state, uint32_t * sols, uint32_t * n_sol)
{
	try
	{
		m_queue.enqueueWriteBuffer(m_blake2b_digest, true, 0, sizeof(crypto_generichash_blake2b_state), &base_state);

		m_queue.enqueueFillBuffer(m_digests[0], &zero, 1, 0, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), 0);
		m_queue.enqueueFillBuffer(m_digests[1], &zero, 1, 0, (NUM_VALUES + NUM_VALUES / 2) * sizeof(digest_t), 0);
		//m_queue.enqueueFillBuffer(m_buckets, &zero, 1, 0, NUM_BUCKETS * sizeof(bucket_t) * EQUIHASH_K, 0);
		m_queue.enqueueFillBuffer(m_new_digest_index, &zero, 1, 0, sizeof(uint32_t), 0);
		m_queue.enqueueFillBuffer(m_blake2b_digest, &zero, 1, 0, sizeof(crypto_generichash_blake2b_state), 0);
		m_queue.enqueueFillBuffer(m_dst_solutions, &zero, 1, 0, 20*NUM_INDICES*sizeof(uint32_t), 0);
		m_queue.enqueueFillBuffer(m_n_solutions, &zero, 1, 0, sizeof(uint32_t), 0);
		m_queue.enqueueFillBuffer(m_src_local_buckets, &zero, 1, 0, NUM_BUCKETS * NUM_INDICES_PER_BUCKET * sizeof(src_local_bucket_t), 0);
		m_queue.enqueueFillBuffer(m_dst_candidates, &zero, 1, 0, (1 << 16)*sizeof(element_t), 0);
		m_queue.enqueueFillBuffer(m_n_candidates, &zero, 1, 0, sizeof(uint32_t), 0);
		m_queue.enqueueFillBuffer(m_elements, &zero, 1, 0, sizeof(uint32_t), 0);

		m_queue.finish();

		// update buffer

		// TODO: Check zcashd for methods for choosing nonces

		/* This is for future loop and output timing
		random_device engine;
		uint64_t start_nonce = uniform_int_distribution<uint64_t>()(engine);
		for (;; start_nonce += m_globalWorkSize)
		{
			auto t = chrono::high_resolution_clock::now();
		*/

		//CL_LOG("Running Solver...");
		// Just send it numbers for now

		//TODO find out why it is segfaulting
		m_zogKernels[0].setArg(0, m_buckets);
		m_zogKernels[0].setArg(1, m_digests[0]);
		m_zogKernels[0].setArg(2, m_blake2b_digest);
		m_zogKernels[0].setArg(3, m_new_digest_index);

		// execute it!
		// Here we assume an 1-Dimensional problem split into local worksizes
		m_queue.enqueueNDRangeKernel(m_zogKernels[0], cl::NullRange, cl::NDRange(1<<20), cl::NDRange(32));

		m_queue.finish();

		uint32_t i = 1;
    	for(i = 1; i < EQUIHASH_K; ++i) {
    		uint32_t n_candidates = 0;

			m_queue.enqueueFillBuffer(m_new_digest_index, &zero, 1, 0, sizeof(uint32_t), 0);

			//m_queue.finish();

			//std::cout << "Step: " << i << "..." << std::endl;

			m_zogKernels[1].setArg(0, m_digests[i % 2]);
			m_zogKernels[1].setArg(1, m_digests[(i - 1)  % 2]);
			m_zogKernels[1].setArg(2, m_buckets);
			m_zogKernels[1].setArg(3, &i);
			m_zogKernels[1].setArg(4, m_new_digest_index);
			m_zogKernels[1].setArg(5, m_src_local_buckets);

			m_queue.enqueueNDRangeKernel(m_zogKernels[1], cl::NullRange, cl::NDRange(NUM_BUCKETS*32), cl::NDRange(32));
			m_queue.finish();
			uint32_t n_digests = 0;
			m_queue.enqueueReadBuffer(m_new_digest_index, true, 0, sizeof(uint32_t), &n_digests);


		}

		uint32_t n_candidates = 0;


		m_zogKernels[2].setArg(0, m_dst_candidates);
		m_zogKernels[2].setArg(1, m_n_candidates);
		m_zogKernels[2].setArg(2, m_buckets);
		m_zogKernels[2].setArg(3, m_digests[0]);
		m_zogKernels[2].setArg(4, m_src_local_buckets);

		m_queue.enqueueNDRangeKernel(m_zogKernels[2], cl::NullRange, cl::NDRange(NUM_BUCKETS*32), cl::NDRange(32));
		m_queue.finish();
		m_queue.enqueueReadBuffer(m_n_candidates, true, 0, sizeof(uint32_t), &n_candidates);

		//m_queue.finish();

	    unsigned largest_bit = 0;
	    uint32_t bit_tmp = n_candidates / 2;
	    while(bit_tmp > 0) {
	        largest_bit++;
	        bit_tmp >>= 1;
	     }

	    uint32_t n_solutions = 0;

		m_queue.enqueueFillBuffer(m_n_solutions, &zero, 1, 0, sizeof(uint32_t), 0);
		m_zogKernels[3].setArg(0, m_dst_solutions);
		m_zogKernels[3].setArg(1, m_n_solutions);
		m_zogKernels[3].setArg(2, m_buckets);
		m_zogKernels[3].setArg(3, m_dst_candidates);
		m_zogKernels[3].setArg(4, m_n_candidates);
		m_zogKernels[3].setArg(5, m_elements);
		m_queue.enqueueNDRangeKernel(m_zogKernels[3], cl::NullRange, cl::NDRange(1<<largest_bit), cl::NDRange(32));
		//pending.push({ start_nonce, buf });
		//buf = (buf + 1) % c_bufferCount;
/*#if CL_VERSION_1_2 && 0
		cl::Event pre_return_event;
		if (!m_opencl_1_1)
			m_queue.enqueueBarrierWithWaitList(NULL, &pre_return_event);
		else
#endif
			m_queue.finish();*/

		// read results - The results don't change... just template for now
		//dst_solutions = (uint32_t*)m_queue.enqueueMapBuffer(m_dst_solutions, true, CL_MAP_READ, 0, 10*NUM_INDICES*sizeof(uint32_t));
		//solutions = (uint32_t*)m_queue.enqueueMapBuffer(m_n_solutions, true, CL_MAP_READ, 0, sizeof(uint32_t));
		m_queue.finish();
		m_queue.enqueueReadBuffer(m_dst_solutions, true, 0, 10*NUM_INDICES*sizeof(uint32_t), dst_solutions);
		m_queue.enqueueReadBuffer(m_n_solutions, true, 0, sizeof(uint32_t), &n_solutions);

		//m_queue.finish();
		
		for(i = 0; i < n_solutions; ++i) {
        	normalize_indices(dst_solutions + (NUM_INDICES*i));
    	}

		//std::cout << "Solutions: " << solutions << std::endl;

		memcpy(sols, dst_solutions, 10*512*sizeof(uint32_t));
		*n_sol = n_solutions;

		/*if (i % 10 == 0){
			CL_LOG("Iteration: " << i << " Result: " <<  *results);
		}*/

			/* This may be useful to get regular outputs
				TODO: Implement this for equihash kernel

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
			*/
		//}

		// not safe to return until this is ready

	}
	catch (cl::Error const& err)
	{
		CL_LOG("CL ERROR:" << get_error_string(err.err()));
		//CL_LOG(err.what() << "(" << err.err() << ")");
	}
}
