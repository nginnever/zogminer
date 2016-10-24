/* MIT License
 *
 * Copyright (c) 2016 Omar Alvarez <omar.alvarez@udc.es>
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

#include <chrono>

#include "gpusolver.h"

#define DEBUG

GPUSolver::GPUSolver() {

	/* Notes
	I've added some extra parameters in this interface to assist with dev, such as
	a kernel string to specify which kernel to run and local/global work sizes.

	The following are private members of the class, but I have them here to specify the
	global work size for now. This will probably be hard-coded later
	*/
	unsigned int z_n = 200;
	unsigned int z_k = 9;
	size_t z_collision_bit_length = z_n / (z_k + 1);
	eh_index z_N = 1 << (z_collision_bit_length + 1);
	//uint32_t global_work_size = z_N;

	//TODO This looks like IND_PER_BUCKET, enough for GPU?
	size_t global_work_size = 1 << 17;
    size_t local_work_size = 32;

	miner = new cl_zogminer();
	miner->listDevices();

	//Generic Things
	std::cout << "Number of Platforms:" << miner->getNumPlatforms << "\n";

	/* Checks each device for memory requirements and sets local/global sizes
	TODO: Implement device logic for equihash kernel
	@params: unsigned platformId
	@params: unsigned localWorkSizes
	@params: unsigned globalWorkSizes
	*/
	GPU = miner->configureGPU(0, local_work_size, global_work_size);
	if(!GPU)
		std::cout << "ERROR: No suitable GPU found! No work will be performed!" << std::endl;

	/*Initialize the kernel, compile it and create buffers
	Currently runs for the gpu-list-gen.c kernel DATA_SIZE=100 times
	TODO: pass base state and nonce's to kernel.
	@params: unsigned _platformId
	@params: unsigned _deviceId
	@params: string& _kernel - The name of the kernel for dev purposes
	*/
	std::vector<std::string> kernels {"initial_bucket_hashing", "bucket_collide_and_hash", "produce_solutions"};
	if(GPU)
		initOK = miner->init(0, 0, kernels);

}

GPUSolver::~GPUSolver() {

	if(GPU)
		miner->finish();

	delete miner;

}

bool GPUSolver::run(unsigned int n, unsigned int k, const eh_HashState& base_state,
		            const std::function<bool(std::vector<unsigned char>)> validBlock,
				const std::function<bool(GPUSolverCancelCheck)> cancelled) {

    if (n == 200 && k == 9) {
        return GPUSolve200_9(base_state, validBlock, cancelled);
    } else {
        throw std::invalid_argument("Unsupported Equihash parameters");
    }

}

bool GPUSolver::GPUSolve200_9(const eh_HashState& base_state,
                 	const std::function<bool(std::vector<unsigned char>)> validBlock,
			const std::function<bool(GPUSolverCancelCheck)> cancelled) {

	/* Run the kernel
	TODO: Optimise and figure out how we want this to go
	@params eh_HashState& base_state - Sends to kernel in a buffer. Will update for specific kernels
	*/
    
	if(GPU && initOK) {
        auto t = std::chrono::high_resolution_clock::now();

		uint32_t n_sol;

    	miner->run(base_state, indices, &n_sol);

		auto d = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t);
		auto milis = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
		if(!counter) {
			sum = 1000.f*n_sol/milis;
		} else { 
			sum += 1000.f*n_sol/milis;
		}
		
		avg = sum/++counter;				
		
		std::cout << "Kernel run took " << milis << " ms. (" << avg << " H/s)" << std::endl;

		size_t checkedSols = 0;
        for (size_t s = 0; s < n_sol; ++s) {
        	++checkedSols;
            std::cout << "Checking solution " << checkedSols << std::endl;
            std::vector<eh_index> index_vector(PROOFSIZE);
            for (size_t i = 0; i < PROOFSIZE; i++) {
            	index_vector[i] = indices[s * PROOFSIZE + i];
            }
            std::vector<unsigned char> sol_char = GetMinimalFromIndices(index_vector, DIGITBITS);
#ifdef DEBUG
            bool isValid;
            EhIsValidSolution(200, 9, base_state, sol_char, isValid);
            std::cout << "is valid: " << isValid << '\n';
            if (!isValid) {
				  //If we find invalid solution bail, it cannot be a valid POW
				  std::cout << "Invalid solution found!" << std::endl;
              	  return false;
            }
#endif
            if (validBlock(sol_char)) {
            	// If we find a POW solution, do not try other solutions
              	// because they become invalid as we created a new block in blockchain.
				  std::cout << "Valid block found!" << std::endl;
              	  return true;
            }
        }

	}

    return false;

}

