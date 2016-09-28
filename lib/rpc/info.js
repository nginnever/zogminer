'use strict'

//database call to get latest block
exports.getblockcount = function getblockcount(args, opt, callback) {
  callback(null, this.node.blockChain.getTopBlock().height);
}