#include "arith_uint256.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "libstratum/StratumClient.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utiltime.h"
#include "version.h"

#include "sodium.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include <csignal>
#include <iostream>
#include <cstdio>

class GPUSolverCancelledException : public std::exception
{
    virtual const char* what() const throw() {
        return "Equihash solver was cancelled";
    }
};

enum GPUSolverCancelCheck
{
    ListGenerationGPU,
    ListSortingGPU
};

GPUSolverCancelledException cancelled;

// The maximum size of the .cl file we read in and compile
#define MAX_SOURCE_SIZE 	(0x200000)

static uint64_t rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#else
#if defined(__amd64__) || defined(__x86_64__)
    uint64_t rax, rdx;
    __asm__ __volatile__("rdtsc" : "=a"(rax), "=d"(rdx) : : );
    return (rdx << 32) | rax;
#elif defined(__i386__) || defined(__i386) || defined(__X86__)
    uint64_t rax;
    __asm__ __volatile__("rdtsc" : "=A"(rax) : : );
    return rax;
#else
#error "Not implemented!"
#endif
#endif
}

//TODO Really wasteful initialize and compile kernels once, not for every iter
bool GPUSolve200_9(const eh_HashState& base_state,
                 	const std::function<bool(std::vector<unsigned char>)> validBlock,
			const std::function<bool(GPUSolverCancelCheck)> cancelled) {

    cl_context context;
    cl_context_properties properties[3];
    cl_kernel kernel;
    cl_command_queue command_queue;
    cl_program program;
    cl_int err;
    cl_uint num_of_platforms=0;
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_uint num_of_devices=0;
    //cl_mem base_state;
    
    size_t global;
    
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
    
    printf("Init Size: %i\n", init_size);
    
    // retreive a list of platforms avaible
    if (clGetPlatformIDs(1, &platform_id, &num_of_platforms)!= CL_SUCCESS)
    {
        printf("Unable to get platform_id\n");
        return 1;
    }
    
    //TODO try to get a supported GPU device
    if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_of_devices) != CL_SUCCESS)
    {
        printf("Unable to get device_id\n");
        return 1;
    }
    
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
    
    // create buffers for the input and ouput
    
    //base_state = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned long) * DATA_SIZE, NULL, NULL);
    /*inputB = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * DATA_SIZE, NULL, NULL);
    output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float) * DATA_SIZE, NULL, NULL);*/
    
    // load data into the input buffer
    //clEnqueueWriteBuffer(command_queue, base_state, CL_TRUE, 0, sizeof(unsigned long) * DATA_SIZE, inputDataA, 0, NULL, NULL);
    /*clEnqueueWriteBuffer(command_queue, inputB, CL_TRUE, 0, sizeof(float) * DATA_SIZE, inputDataB, 0, NULL, NULL);*/
    
    // set the argument list for the kernel command
    clSetKernelArg(kernel, 0, sizeof(eh_HashState), &base_state);
    //clSetKernelArg(kernel, 1, sizeof(cl_mem), &inputB);
    //clSetKernelArg(kernel, 2, sizeof(cl_mem), &output);
    
    global=init_size;
    //global=DATA_SIZE;
    
    printf("Starting list generation in GPU...\n");
    
    // enqueue the kernel command for execution
    clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL);
    clFinish(command_queue);
    
    printf("List generation in GPU done...\n");
    if (cancelled(ListGenerationGPU)) throw cancelled;

    
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
    //clReleaseMemObject(base_state);
    /*clReleaseMemObject(inputB);
    clReleaseMemObject(output);*/
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);

    //TODO Check this, now it is a dummy value
    return false;

}

bool GPUSolver(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<unsigned char>)> validBlock,
const std::function<bool(GPUSolverCancelCheck)> cancelled) {

    if (n == 200 && k == 9) {
        return GPUSolve200_9(base_state, validBlock, cancelled);
    } else {
        throw std::invalid_argument("Unsupported Equihash parameters");
    }

}


std::string HelpMessageMiner()
{
    string strUsage;
    strUsage += HelpMessageGroup(_("Options:"));
    strUsage += HelpMessageOpt("-?", _("This help message"));

    strUsage += HelpMessageGroup(_("Mining pool options:"));
    strUsage += HelpMessageOpt("-stratum=<url>", _("Mine on the Stratum server at <url>"));
    strUsage += HelpMessageOpt("-user=<user>",
                               strprintf(_("Username for Stratum server (default: %u)"), "x"));
    strUsage += HelpMessageOpt("-password=<pw>",
                               strprintf(_("Password for Stratum server (default: %u)"), "x"));

    strUsage += HelpMessageGroup(_("Debugging/Testing options:"));
    string debugCategories = "cycles, pow, stratum"; // Don't translate these
    strUsage += HelpMessageOpt("-debug=<category>", strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + ". " +
        _("If <category> is not supplied, output all debugging information.") + " " + _("<category> can be:") + " " + debugCategories + ".");
    strUsage += HelpMessageOpt("-printtoconsole", _("Send trace/debug info to console instead of debug.log file"));
    strUsage += HelpMessageOpt("-regtest", _("Enter regression test mode, which uses a special chain in which blocks can be "
                                             "solved instantly. This is intended for regression testing tools and app development."));
    strUsage += HelpMessageOpt("-testnet", _("Use the test network"));

    return strUsage;
}

std::string LicenseInfo()
{
    return FormatParagraph(_("Copyright (C) 2009-2015 The Bitcoin Core Developers")) + "\n" +
           FormatParagraph(strprintf(_("Copyright (C) 2015-%i The Zcash Developers"), COPYRIGHT_YEAR)) + "\n" +
           FormatParagraph(strprintf(_("Copyright (C) %i Jack Grigg <jack@z.cash>"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(_("This is experimental software.")) + "\n" +
           "\n" +
           FormatParagraph(_("Distributed under the MIT software license, see the accompanying file COPYING or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n";
}

void test_mine(int n, int k, uint32_t d)
{
    CBlock pblock;
    pblock.nBits = d;
    arith_uint256 hashTarget = arith_uint256().SetCompact(d);

    while (true) {
        // Hash state
        crypto_generichash_blake2b_state state;
        EhInitialiseState(n, k, state);

        // I = the block header minus nonce and solution.
        CEquihashInput I{pblock};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

        // Find valid nonce
        int64_t nStart = GetTime();
        uint64_t start_cycles = rdtsc();
        while (true) {
            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = state;
            crypto_generichash_blake2b_update(&curr_state,
                                              pblock.nNonce.begin(),
                                              pblock.nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            LogPrint("pow", "Running Equihash solver with nNonce = %s\n",
                     pblock.nNonce.ToString());

            std::function<bool(std::vector<unsigned char>)> validBlock =
                    [&pblock, &hashTarget, &nStart, &start_cycles]
                    (std::vector<unsigned char> soln) {
                // Write the solution to the hash and compute the result.
                pblock.nSolution = soln;

                if (UintToArith256(pblock.GetHash()) > hashTarget) {
                    return false;
                }

                // Found a solution
                uint64_t stop_cycles = rdtsc();
                LogPrintf("ZcashMiner:\nproof-of-work found\nprevHash: %s\n    hash: %s\n  target: %s\nduration: %s\n",
                          pblock.hashPrevBlock.GetHex(),
                          pblock.GetHash().GetHex(),
                          hashTarget.GetHex(),
                          (GetTime() - nStart));
                LogPrint("cycles", "  cycles: %2.2f Mcycles\n",
                         (double)(stop_cycles - start_cycles) / (1UL << 20));

                return true;
            };
            std::function<bool(GPUSolverCancelCheck)> cancelled =
                    [](GPUSolverCancelCheck pos) {
                return false;
            };
            try {
                uint64_t solve_start = rdtsc();
                //bool foundBlock = EhOptimisedSolve(n, k, curr_state, validBlock, cancelled);
		bool foundBlock = GPUSolver(n, k, curr_state, validBlock, cancelled);
                uint64_t solve_end = rdtsc();
                LogPrint("cycles", "Solver took %2.2f Mcycles\n\n",
                         (double)(solve_end - solve_start) / (1UL << 20));
                // If we find a valid block, we rebuild
                if (foundBlock) {
                    break;
                }
            } catch (GPUSolverCancelledException&) {
                LogPrint("pow", "Equihash solver cancelled\n");
            }

            if ((UintToArith256(pblock.nNonce) & 0xffff) == 0xffff) {
                break;
            }
            pblock.nNonce = ArithToUint256(UintToArith256(pblock.nNonce) + 1);
        }

        pblock.hashPrevBlock = pblock.GetHash();
    }
}

static ZcashStratumClient* scSig;
extern "C" void stratum_sigint_handler(int signum) {if (scSig) scSig->disconnect();}

int main(int argc, char* argv[])
{
    ParseParameters(argc, argv);

    // Process help and version first
    if (mapArgs.count("-?") || mapArgs.count("-h") ||
        mapArgs.count("-help") || mapArgs.count("-version")) {
        std::string strUsage = _("Zcash Miner") + " " +
                               _("version") + " " + FormatFullVersion() + "\n";

        if (mapArgs.count("-version")) {
            strUsage += LicenseInfo();
        } else {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  zcash-miner [options]                     " + _("Start Zcash Miner") + "\n";

            strUsage += "\n" + HelpMessageMiner();
        }

        std::cout << strUsage;
        return 1;
    }

    // Zcash debugging
    fDebug = !mapMultiArgs["-debug"].empty();
    fPrintToConsole = GetBoolArg("-printtoconsole", false);

    // Check for -testnet or -regtest parameter
    // (Params() calls are only valid after this clause)
    if (!SelectParamsFromCommandLine()) {
        std::cerr << "Error: Invalid combination of -regtest and -testnet." << std::endl;
        return 1;
    }

    // Initialise libsodium
    if (init_and_check_sodium() == -1) {
        return 1;
    }

    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    LogPrintf("Zcash Miner version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);

    // Start the mining operation
    std::string stratum = GetArg("-stratum", "");
    if (!stratum.empty() || GetBoolArg("-stratum", false)) {
        if (stratum.compare(0, 14, "stratum+tcp://") != 0) {
            std::cerr << "Error: -stratum must be a stratum+tcp:// URL." << std::endl;
            return false;
        }

        std::string host;
        std::string port;
        std::string stratumServer = stratum.substr(14);
        size_t delim = stratumServer.find(':');
        if (delim != std::string::npos) {
            host = stratumServer.substr(0, delim);
            port = stratumServer.substr(delim+1);
        }
        if (host.empty() || port.empty()) {
            std::cerr << "Error: -stratum must contain a host and port." << std::endl;
            return false;
        }

        ZcashMiner miner(GetArg("-genproclimit", 1));
        ZcashStratumClient sc {
            &miner, host, port,
            GetArg("-user", "x"),
            GetArg("-password", "x"),
            0, 0
        };

        miner.onSolutionFound([&](const EquihashSolution& solution) {
            return sc.submit(&solution);
        });

        scSig = &sc;
        signal(SIGINT, stratum_sigint_handler);

        while(sc.isRunning()) {
            MilliSleep(1000);
        }
    } else {
        std::cout << "Running the test miner" << std::endl;
        test_mine(
            Params().EquihashN(),
            Params().EquihashK(),
            0x200f0f0f);
    }

    return 0;
}
