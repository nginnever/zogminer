# ZCash-gpu-miner
An OpenCL node.js [Zcash](https://z.cash) miner solving [Equihash](https://www.internetsociety.org/sites/default/files/blogs-media/equihash-asymmetric-proof-of-work-based-generalized-birthday-problem.pdf) PoW.

## Project Status

**Status:** *In active development*

Check the project's [roadmap](https://github.com/nginnever/zogminer/blob/master/ROADMAP.md) to see what's happening at the moment and what's planned next.

[![Project Status](https://badge.waffle.io/nginnever/zogminer.svg?label=In%20Progress&title=In%20Progress)](https://waffle.io/nginnever/zogminer)
[![Build Status](https://travis-ci.org/nginnever/zogminer.svg?branch=master)](https://travis-ci.org/nginnever/zogminer)

## Building

First make sure OpenCL is correctly installed in your system. If you are having issues, take a look at our [Ubuntu Guide](https://github.com/nginnever/zogminer/wiki/Ubuntu-16.04-Setup) or [Fedora Guide](https://github.com/nginnever/zogminer/wiki/Fedora-24-Setup).

### Unix

Install the dependencies. On Debian/Ubuntu-based systems:

```
$ sudo apt-get install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python \
      zlib1g-dev wget bsdmainutils automake opencl-headers \
      mesa-common-dev
```

On Fedora-based systems:

```
$ sudo dnf install \
      git pkgconfig automake autoconf ncurses-devel python wget \
      gtest-devel gcc gcc-c++ libtool patch opencl-headers mesa-libGL-devel
```

Fetch our repository with git and run fetch-params.sh like so:

```
$ git clone https://github.com/nginnever/zogminer.git
$ cd zogminer/
$ ./zcutil/fetch-params.sh
```
Ensure you have successfully installed all system dependencies as described above. Then run the build, e.g.:

```
$ ./zcutil/build.sh -j$(nproc)
```

This should compile our dependencies and build zcash-miner. (Note: if you don't have nproc, then substitute the number of your processors.)

## Running

# Run GPU test miner

```
$ ./src/zcash-miner -G
```

Usage:

```
$ ./src/zcash-miner -help
```

# Stratum

```
./src/zcash-miner -G -stratum="stratum+tcp://<address>:<port>" -user=<user> -password=<pass>
```

# Solo mine ZCash

First create or modify ```~/.zcash/zcash.conf``` with the following:

```
testnet=1
addnode=betatestnet.z.cash
rpcuser=username
rpcpassword=password
gen=1
GPU=1
deviceid=<device> (default: 0)
```

Next run zcashd:

```
./src/zcashd
```
