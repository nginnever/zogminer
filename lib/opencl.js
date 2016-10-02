'use strict'

const nooocl = require('nooocl')
const CLHost = nooocl.CLHost
const CLContext = nooocl.CLContext
const CLCommandQueue = nooocl.CLCommandQueue
const clCreateProgramWithSource = nooocl.clCreateProgramWithSource
const NDRange = nooocl.NDRange
const CLBuffer = nooocl.CLBuffer

let device

var opencl = function () {

}

opencl.prototype.connect = (device) => {
  if (device === undefined) {
    this.device = 'gpu'
  }

  var host = CLHost.createV12()
  host = new CLHost(1.2)
  
  var platform = host.getPlatforms()[0]

  if(this.device === 'gpu') {
    this.device = platform.gpuDevices()[0]
  } else {
    this.device = platform.cpuDevices()[0]
  }

}

exports.opencl = opencl