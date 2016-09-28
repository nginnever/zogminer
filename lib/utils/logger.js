'use strict'

const winston = require('winston')

var loggingLevels = {
  levels: {
    // =========================================================================
    // Set any of these to 1 to enable the corresponding debug channel.
    // -------------------------------------------------------------------------
    netdbg: 0, // Network debugging
    bchdbg: 0, // Block chain debugging
    rpcdbg: 0, // JSON RPC debugging
    scrdbg: 0, // Script interpreter debugging
    // =========================================================================
    debug: 1,
    info: 10,
    notice: 20,
    warn: 30,
    error: 40,
    crit: 50,
    alert: 60,
    emerg: 70
  },
  colors: {
    netdbg: 'blue',
    bchdbg: 'blue',
    rpcdbg: 'blue',
    scrdbg: 'blue',
    debug: 'blue',
    info: 'green',
    notice: 'cyan',
    warn: 'yellow',
    error: 'red',
    crit: 'red',
    alert: 'yellow',
    emerg: 'red'
  }
}

var months = ['Jan', 'Feb', 'Mar', 'Apr',
              'May', 'Jun', 'Jul', 'Aug',
              'Sep', 'Oct', 'Nov', 'Dec']

var pad = function (n) {
  return n < 10 ? '0' + n.toString(10) : n.toString(10)
}

var pad3 = function (n) {
  var num = n.toString(10)
  while (num.length < 3) num = '0' + num
  return num
}

var timestamp = function () {
  var d = new Date()
  var time = [
    pad(d.getHours()),
    pad(d.getMinutes()),
    pad(d.getSeconds())
  ].join(':')
  time += '.' + pad3(d.getMilliseconds())

  return [d.getDate(), months[d.getMonth()], time].join(' ')
}

var logger = module.exports = new (winston.Logger)({
  transports: [
    new (winston.transports.Console)({
      colorize: true,
      level: 'debug',
      timestamp: timestamp
    })
  ],
  levels: loggingLevels.levels,
  level: 'debug'
})

winston.addColors(loggingLevels.colors)

exports.disable = function () {
  this.logger.remove(winston.transports.Console)
}

//logger.extend(exports)