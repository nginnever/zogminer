'use strict'

const Binary = require('binary')

for (var i in Binary) {
  if(Binary.hasOwnProperty(i)) {
    exports[i] = Binary[i]
  }
}

// exports.put.prototype.varint = function (i) {
//   if (i < 0xFD) {
//     // unsigned char
//     this.word8(i)
//   } else if (i <= 1<<16) {
//     this.word8(0xFD)
//     // unsigned short (LE)
//     this.word16le(i)
//   } else if (i <= 1<<32) {
//     this.word8(0xFE)
//     // unsigned int (LE)
//     this.word32le(i)
//     } else {
//     this.word8(0xFF)
//     // unsigned long long (LE)
//     this.word64le(i)
//   }
// }