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
	uint32_t global_work_size = z_N;

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
	GPU = miner->configureGPU(0, 1, global_work_size);
	if(!GPU)
		std::cout << "ERROR: No suitable GPU found! No work will be performed!" << std::endl;

	/*Initialize the kernel, compile it and create buffers
	Currently runs for the gpu-list-gen.c kernel DATA_SIZE=100 times
	TODO: pass base state and nonce's to kernel.
	@params: unsigned _platformId
	@params: unsigned _deviceId
	@params: string& _kernel - The name of the kernel for dev purposes
	*/
	if(GPU)
		miner->init(0,0,"list_gen");

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

//TODO Really wasteful initialize and compile kernels once, not for every iter
bool GPUSolver::GPUSolve200_9(const eh_HashState& base_state,
                 	const std::function<bool(std::vector<unsigned char>)> validBlock,
			const std::function<bool(GPUSolverCancelCheck)> cancelled) {

	/* Run the kernel
	TODO: Optimise and figure out how we want this to go
	@params ulong& headerIn - Sends to kernel in a buffer. Will update for specific kernels
	*/
    ulong foo = 3;

	if(GPU) {
        auto t = std::chrono::high_resolution_clock::now();

    	miner->run(foo);

		auto d = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t);
		auto milis = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
		std::cout << "Kernel run took " << milis << " ms. (" << 1000.f/milis << " H/s)" << std::endl;
	}

    //TODO Check this, now it is a dummy value
    return false;

}

