var path = require('path');
var yanop = require('yanop');
var fs = require('fs');
var colors = require('colors');
var mkdirp = require('mkdirp');

// Load bitcoinjs-server
var Zcash = require('../lib/zcash');
var logger = require('../lib/utils/logger');
var Settings = require('../lib/settings').Settings;

var mods = [];

var getConfig = exports.getConfig = function getConfig(initConfig) {
  if ("object" !== typeof initConfig) {
    initConfig = {};
  }

  // Command-line arguments parsing
  var opts = yanop.simple({
    config: {
      type: yanop.string,
      short: 'c',
      description: 'Configuration file'
    },
    homedir: {
      type: yanop.string,
      description: 'Path to BitcoinJS home directory (default: ~/.bitcoinjs/)'
    },
    datadir: {
      type: yanop.string,
      description: 'Data directory, relative to home dir (default: .)'
    },
    addnode: {
      type: yanop.list,
      description: 'Add a node to connect to'
    },
    forcenode: {
      type: yanop.list,
      description: 'Always maintain a connection to this node'
    },
    connect: {
      type: yanop.string,
      description: 'Connect only to the specified node'
    },
    nolisten: {
      type: yanop.flag,
      description: 'Disable incoming connections'
    },
    livenet: {
      type: yanop.flag,
      description: 'Use the regular network (default)'
    },
    testnet: {
      type: yanop.flag,
      description: 'Use the test network'
    },
    port: {
      type: yanop.scalar,
      short: 'p',
      description: 'Port to listen for incoming connections'
    },
    rpcuser: {
      type: yanop.string,
      description: 'Username for JSON-RPC connections'
    },
    rpcpassword: {
      type: yanop.string,
      description: 'Password for JSON-RPC connections'
    },
    rpcport: {
      type: yanop.scalar,
      description: 'Listen for JSON-RPC connections on <port> (default: 8432)'
    },
    netdbg: {
      type: yanop.flag,
      description: 'Enable networking debug messages'
    },
    bchdbg: {
      type: yanop.flag,
      description: 'Enable block chain debug messages'
    },
    rpcdbg: {
      type: yanop.flag,
      description: 'Enable JSON RPC debug messages'
    },
    scrdbg: {
      type: yanop.flag,
      description: 'Enable script parser/interpreter debug messages'
    },
    mods: {
      type: yanop.string,
      short: 'm',
      description: 'Comma-separated list of mods to load'
    },
    noverify: {
      type: yanop.flag,
      description: 'Disable all tx/block verification'
    },
    noverifyscripts: {
      type: yanop.flag,
      description: 'Disable tx scripts verification'
    }
  });

  // Print welcome message
  if (initConfig.welcome) {
    require("./welcome");
  }

  //
  // Configuration file
  //
  logger.info('Loading configuration');

  // Calculate config file path
  var configPath, homeDir;
  if (opts.config) {
    // Explicit config file path provided via flag
    configPath = path.resolve(opts.config);
  } else {
    var defHome = Settings.getDefaultHome() + (opts.testnet ? '/testnet' : '');
    homeDir = opts.homedir ? path.resolve(opts.homedir) : defHome;
    configPath = path.resolve(homeDir, './settings');

    // DEPRECATED: Search in source tree for daemon/settings.js
    try {
      require.resolve('./settings');
      configPath = './settings';
    } catch (e) {}
  }
  try {
    // Check if config file exists (throws an exception otherwise)
    require.resolve(configPath);
  } catch (e) {
    if (configPath.substr(-3) !== '.js') configPath += '.js';

    var exampleConfigPath = path.resolve(__dirname, './settings.example.js');
    var targetConfigPath = path.resolve(__dirname, configPath);

    try {
      // Create config/home directory
      mkdirp.sync(path.dirname(configPath));

      // Copy example config file
      fs.writeFileSync(targetConfigPath, fs.readFileSync(exampleConfigPath));

      // Test config file
      require.resolve(configPath);

      logger.info('Automatically created config file');
      util.puts(
        "\n" +
          "| BitcoinJS created a new default config file at:\n" +
          "| " + targetConfigPath + "\n" +
          "| \n" +
          "| Please edit it to suit your requirements, for example to enable JSON-RPC.\n".bold);
    } catch (e) {
      logger.error('Unable to automatically create config file!');
      util.puts(
        "\n" +
          "| BitcoinJS was unable to locate or create a config file at:\n" +
          "| " + targetConfigPath + "\n" +
          "| \n" +
          "| Please create a config file in this location or provide the correct path\n" +
          "| to your config using the --config=/path/to/settings.js option.\n" +
          "| \n" +
          "| To get started you can copy the example config file from here:\n" +
          "| " + exampleConfigPath + "\n");
      process.exit(1);
    }
  }

  var cfg;
  try {
    cfg = global.cfg = new Settings();
    cfg.homedir = homeDir;
    var returnedCfg = require(configPath);

    if (returnedCfg instanceof Settings) {
      cfg = returnedCfg;
    }
  } catch (e) {
    logger.error('Error while loading configuration file:\n\n'+
                 (e.stack ? e.stack : e.toString()));
    process.exit(1);
  }

  if (!(cfg instanceof Bitcoin.Settings)) {
    logger.error('Configuration file did not provide a valid Settings object.\n');
    process.exit(1);
  }

  // Apply configuration from the command line
  if (opts.homedir) {
    cfg.homedir = opts.homedir;
  }
  if (opts.datadir) {
    cfg.datadir = opts.datadir;
  }
  if (opts.addnode.length) {
    cfg.network.initialPeers = cfg.network.initialPeers.concat(opts.addnode);
  }
  if (opts.forcenode.length) {
    cfg.network.initialPeers = cfg.network.forcePeers.concat(opts.forcenode);
  }
  if (opts.connect) {
    if (opts.connect.indexOf(',') != -1) {
      opts.connect = opts.connect.split(',');
    }
    cfg.network.connect = opts.connect;
  }
  if (opts.nolisten) {
    cfg.network.noListen = opts.nolisten;
  }
  if (opts.livenet) {
    cfg.setLivenetDefaults();
  } else if (opts.testnet) {
    cfg.setTestnetDefaults();
  }
  if (opts.port) {
    opts.port = +opts.port;
    if (opts.port > 65535 || opts.port < 0) {
      logger.error('Invalid port setting: "'+opts.port+'"');
    } else {
      cfg.network.port = opts.port;
    }
  }
  if (opts.rpcuser) {
    cfg.jsonrpc.enable = true;
    cfg.jsonrpc.username = opts.rpcuser;
  }
  if (opts.rpcpassword) {
    cfg.jsonrpc.enable = true;
    cfg.jsonrpc.password = opts.rpcpassword;
  }
  if (opts.rpcport) {
    opts.rpcport = +opts.rpcport;
    if (opts.port > 65535 || opts.port < 0) {
      logger.error('Invalid port setting: "'+opts.rpcport+'"');
    } else {
      cfg.jsonrpc.port = opts.rpcport;
    }
  }
  if (opts.netdbg) {
    logger.logger.levels.netdbg = 1;
  }
  if (opts.bchdbg) {
    logger.logger.levels.bchdbg = 1;
  }
  if (opts.rpcdbg) {
    logger.logger.levels.rpcdbg = 1;
  }
  if (opts.scrdbg) {
    logger.logger.levels.scrdbg = 1;
  }
  if (opts.mods) {
    cfg.mods = (("string" === typeof cfg.mods) ? cfg.mods+',' : '') +
      opts.mods;
  }
  if (opts.noverify) {
    cfg.verify = false;
  }
  if (opts.noverifyscripts) {
    cfg.verifyScripts = false;
  }

  return cfg;
};

var createNode = exports.createNode = function createNode(initConfig) {
  var cfg = getConfig(initConfig);

  // Return node object
  var node = new Bitcoin.Node(cfg);

  return node;
};