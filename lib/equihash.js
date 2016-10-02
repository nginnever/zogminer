'use strict'

const blake2 = require('blake2')

let eh_index
let eh_trunc

var Equihash = function (N, K) {
  var IndicesPerHashOutput = 512/N
  var HashOutput = IndicesPerHashOutput*N/8
  var CollisionBitLength = N/(K+1)
  var CollisionByteLength = (CollisionBitLength+7)/8
  var HashLength = (K+1)*CollisionByteLength
  // figure out sizeof(eh_index)
  var FullWidth = 2*CollisionByteLength+eh_index.length*(1 << (K-1))
  // figure out max
  var TrunctatedWidth = max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K-1)))
  var FinalTruncatedWidth = max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K)))
  var SolutionWidth = (1 << K)*(CollisionBitLength+1)/8



Equihash.prototype.InitializeState = (base_state) => {

}

Equihash.prototype.BasicSolve = () => {

}

exports.Equihash = Equihash