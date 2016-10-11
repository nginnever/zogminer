
#include <stdio.h>
#include <iostream>
#include "cl_zogminer.h"

using namespace std;

int main(void)
{
  cl_zogminer miner;
  miner.listDevices();

  //Generic Things
  cout << "Number of Platforms:" << miner.getNumPlatforms << "\n";

  // Just prints memory for the time being. Was being used to verify mem requirements
  miner.configureGPU(1);

  //Initialize the kernel, compile etc...
  miner.init(1,1);

  // I'm yet to set up the correct buffers...

}
