
#include <stdio.h>
#include <iostream>
#include "cl_zogminer.h"

using namespace std;

int main(void)
{
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

  cl_zogminer miner;
  miner.listDevices();

  //Generic Things
  cout << "Number of Platforms:" << miner.getNumPlatforms << "\n";

  /* Checks each device for memory requirements and sets local/global sizes
    TODO: Implement device logic for equihash kernel
    @params: unsigned platformId
    @params: unsigned localWorkSizes
    @params: unsigned globalWorkSizes
  */
  miner.configureGPU(0, 1, global_work_size);

  /*Initialize the kernel, compile it and create buffers
    Currently runs for the gpu-list-gen.c kernel DATA_SIZE=100 times
    TODO: pass base state and nonce's to kernel.
    @params: unsigned _platformId
    @params: unsigned _deviceId
    @params: string& _kernel - The name of the kernel for dev purposes
  */
  miner.init(0,0,"list_gen");

  /* Run the kernel
    TODO: Optimise and figure out how we want this to go
    @params ulong& headerIn - Sends to kernel in a buffer. Will update for specific kernels
  */
  ulong foo = 3;
  miner.run(foo);

  miner.finish();

}
