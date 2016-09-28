const net = require('net')
const Binary = require('./utils/binary');
const logger = require('./utils/logger');

var Peer = function (host, port, services) {
  if ("string" === typeof host) {
    if (host.indexOf(':') && !port) {
      var parts = host.split(':');
      host = parts[0];
      port = parts[1];
    }
    this.host = host;
    this.port = +port || 8333;
  } else if (host instanceof Peer) {
    this.host = host.host;
    this.port = host.port;
  } else if (Buffer.isBuffer(host)) {
    // Parse as IPv6; Bitcoin sends all addresses as IPv6, using the standard mapping (http://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses) if only IPv4
    this.host = host.toString('hex').match(/(.{1,4})/g).join(':');
    this.port = +port || 8333;
  } else {
    throw new Error('Could not instantiate peer, invalid parameter type: ' +
                    typeof host);
  }

  this.services = (services) ? services : null;
  this.lastSeen = 0;
};

Peer.prototype.createConnection = function () {
  var c = net.createConnection(this.port, this.host);
  return c;
};

Peer.prototype.getHostAsBuffer = function () {
  return new Buffer(this.host.split('.'));
};

Peer.prototype.toString = function () {
  return this.host + ":" + this.port;
};

Peer.prototype.toBuffer = function () {
  var put = Binary.put();
  put.word32le(this.lastSeen);
  put.word64le(this.services);
  put.put(this.getHostAsBuffer());
  put.word16be(this.port);
  return put.buffer();
};

exports.Peer = Peer