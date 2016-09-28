'use strict'

const fs = require('fs')
const path = require('path')
const mkdirp = require('mkdirp');
const logger = require('./utils/logger') // logging
const events = require('events')

const Peer = require('./peer').Peer
const Storage = require('./storage').Storage
var Settings = require('./settings').Settings;
var Connection = require('./connection').Connection;
var BlockChain = require('./blockchain').BlockChain;
var Transaction = require('./schema/transaction').Transaction;
var TransactionStore = require('./transactionstore').TransactionStore;
var TransactionSender = require('./transactionsender').TransactionSender;
var PeerManager = require('./peermanager').PeerManager;
var BlockChainManager = require('./blockchainmanager').BlockChainManager;
var JsonRpcServer = require('./rpc/jsonrpcserver').JsonRpcServer;
var Util = require('./util');

var Node = function Node(cfg) {
  events.EventEmitter.call(this);

  var self = this;

  if (!cfg) {
    cfg = new Settings();
  }

  this.cfg = cfg;
  this.peers = [];
  this.connections = [];
  this.nonce = Util.generateNonce();

  // Protocol version
  this.version = 70000;

  this.state = null;
  this.running = false;

  if (cfg.mods) {
    var modNames = cfg.mods.split(',');
    for (var i in modNames) {
      var name = modNames[i].trim();

      if ("string" !== typeof name || name.length == 0) continue;

      try {
        var mod = require('../mods/'+name+'/'+name);
        if ("function" === typeof mod.load) {
          mod.load(this);
        }

        if ("function" === typeof mod.init) {
          this.addListener('init', function () {
            mod.init(self);
          });
        }
      } catch (e) {
        logger.warn('Failed to load mod "'+name+'": '+
                    (e.stack ? e.stack : e.toString()));
      }
    }
  }

  var existsSync = fs.existsSync || path.existsSync;

  // Try and create homedir if it doesn't exist
  var homeDir = this.cfg.getHomeDir();
  try {
    if (!existsSync(homeDir)) {
      mkdirp.sync(homeDir, 0755);
    }
  } catch (err) {
    logger.error("Could not create datadir '"+homeDir+"': " +
                 (err.stack ? err.stack : err));
    return;
  }

  // Try and create datadir if it doesn't exist
  var dataDir = this.cfg.getDataDir();
  try {
    if (!existsSync(dataDir)) {
      mkdirp.sync(dataDir, 0755);
    }
  } catch (err) {
    logger.error("Could not create datadir '"+dataDir+"': " +
                 (err.stack ? err.stack : err));
    return;
  }

  var storageUri = this.cfg.storage.uri;
  if (!storageUri) {
    storageUri = 'leveldb://' + dataDir + '/leveldb/';
  }

  // Initialize components
  try {
    this.storage = Storage.get(storageUri);
    this.blockChain = new BlockChain(this.storage, this.cfg);
    this.txStore = new TransactionStore(this);
    this.txSender = new TransactionSender(this);
    this.peerManager = new PeerManager(this);
    this.bcManager = new BlockChainManager(this.blockChain,
                                           this.peerManager);
    this.rpcServer = new JsonRpcServer(this);

    this.addListener('stateChange', this.handleStateChange.bind(this));
    this.setupStateTransitions();
    this.setupCrossMessaging();
  } catch (err) {
    logger.error("Initialization " +
                 (err.stack ? err.stack :
                  ("error: "+err.toString())));
  }
};

util.inherits(Node, events.EventEmitter);

/**
 * Setup triggers for automatically switching states.
 *
 * The Node class automatically switches to different state if a certain
 * event happens while it is in a certain state.
 *
 * During startup this is what causes the Node to progress through its
 * various startup stages.
 */
Node.prototype.setupStateTransitions = function ()
{
  var self = this;

  // When the BlockChain object has finished initializing,
  // start connecting to peers
  this.blockChain.addListener('initComplete', function () {
    if (self.state == 'init') {
      self.setState('netConnect');
    }
  });

  // When the first peer is fully connected, start the block
  // chain download
  this.peerManager.addListener('netConnected', function () {
    if (self.state == 'netConnect') {
      self.setState('blockDownload');
    }
  });
};

/**
 * Setup cross component messaging.
 *
 * Some components of the node have to talk to one another. The node
 * sets up these channels using this function after all the components
 * have been instantiated.
 */
Node.prototype.setupCrossMessaging = function ()
{
  // When a transaction gets included into the block chain, it and
  // conflicting transactions must be removed from the memory pool.
  this.blockChain.addListener('txAdd', this.txStore.handleTxAdd.bind(this.txStore));

  // We only want to rebroadcast once per block at the most, so the
  // sender class needs to know when a new block arrives.
  this.blockChain.addListener('blockAdd', this.txSender.handleBlock.bind(this.txSender));
};

Node.prototype.start = function () {
  this.setState('init');
};

Node.prototype.setState = function (newState) {
  var oldState = this.state;

  // Don't allow switching to init state unless we are uninitialized
  if (newState == 'init' && oldState !== null) {
    return;
  }

  this.state = newState;

  this.emit('stateChange', {oldState: oldState, newState: newState});
};

Node.prototype.handleStateChange = function (e) {
  // We consider the node to be "running" if it is in one of these states
  this.running = !!~['netConnect', 'blockDownload', 'default'].indexOf(e.newState);

  // Define what happens when we leave certain states
  switch (e.oldState) {}

  // Define what happens when we enter certain states
  switch (e.newState) {
  case 'init':
    this.blockChain.init();
    this.emit('init');
    break;

  case 'netConnect':
    this.peerManager.enable();
    this.txSender.enable();
    this.bcManager.enable();
    this.rpcServer.enable();
    break;

  // TODO: Merge netConnect and blockDownload into new state "active"
  case 'blockDownload':
    break;
  }
};

Node.prototype.addPeer = function (peer) {
  this.peerManager.addPeer(peer);
};

Node.prototype.addConnection = function (conn) {
  conn.addListener('inv', this.handleInv.bind(this));
  conn.addListener('block', this.handleBlock.bind(this));
  conn.addListener('tx', this.handleTx.bind(this));
  conn.addListener('getdata', this.handleGetdata.bind(this));
  conn.addListener('getblocks', this.handleGetblocks.bind(this));
  conn.addListener('getheaders', this.handleGetheaders.bind(this));
};

Node.prototype.getPeerManager = function () {
  return this.peerManager;
};

Node.prototype.getStorage = function () {
  return this.storage;
};

Node.prototype.getBlockChain = function () {
  return this.blockChain;
};

Node.prototype.getTxStore = function () {
  return this.txStore;
};

Node.prototype.getTxSender = function () {
  return this.txSender;
};

Node.prototype.handleInv = function (e) {
  var self = this;
  var invs = e.message.invs;
  var toCheck = invs.length;
  var unknownInvs = new Array(invs.length);

  for (var i = 0; i < invs.length; i++) {
    var method;
    switch (invs[i].type) {
    case 1: // Transaction
      toCheck--;

      // No need to request the transaction if we're not processing anyway
      if (!this.blockChain.isPastCheckpoints()) {
        continue;
      }

      // Check whether we know this transaction
      if (!this.txStore.isKnown(invs[i].hash)) {
        unknownInvs[i] = invs[i];
      }

      break;
    case 2: // Block

      // This will asynchronously check all the blocks and transactions.
      // Finally, the last callback will trigger the 'getdata' request.
      this.blockChain.knowsBlock(invs[i].hash, (function (err, known) {
        toCheck--;

        if (err) {
          logger.error('Node.handleInv(): Could not check inv '+
                       Util.formatHashAlt(invs[this].hash)+': '+
                       (err.stack ? err.stack : err));
        } else {
          if (!known) {
            unknownInvs[this] = invs[this];
          } else if (self.blockChain.isOrphan(invs[this].hash)) {
            // This peer knows one of our orphan blocks. Execute a getblocks
            // request for the path to this block.
            self.bcManager.startDownload(invs[this].hash, null, e.conn);
          }
        }

        if (toCheck === 0 && unknownInvs.length) {
          unknownInvs = unknownInvs.filter(function (hash) { return hash; });
          e.conn.sendGetData(unknownInvs);
        }
      }).bind(i));
      break;
    default: // Unknown type
      continue;
    }
  }

  if (toCheck === 0 && unknownInvs.length) {
    unknownInvs = unknownInvs.filter(function (hash) { return hash; });
    e.conn.sendGetData(unknownInvs);
  }
};

Node.prototype.handleBlock = function (e) {
  var txs = e.message.txs;
  var block = this.blockChain.makeBlockObject({
    "version": e.message.version,
    "prev_hash": e.message.prev_hash,
    "merkle_root": e.message.merkle_root,
    "timestamp": e.message.timestamp,
    "bits": e.message.bits,
    "nonce": e.message.nonce
  });

  block.hash = block.calcHash();
  block.size = e.message.size;

  var callback = this.handleBlockAddCallback.bind(block);

  this.blockChain.add(block, txs, callback);
};

Node.prototype.handleBlockAddCallback = function (err) {
  var block = this;

  if (err) {
    logger.error("Error adding block "+
                 Util.formatHashAlt(block.hash)+": "+
                 (err.stack ? err.stack : err.toString()));
  };
};

Node.prototype.handleTx = function (e) {
  var message = e.message;
  delete message.command;

  // Ignore memory transactions until the block chain has reached the last
  // checkpoint at least.
  if (!this.blockChain.isPastCheckpoints()) {
    return;
  }

  var tx = new Transaction(message);

  if (this.txStore.isKnown(tx.getHash())) {
    return;
  }

  this.txStore.add(tx, function (err) {
    if (err) {
      if (err.name === "MissingSourceError") {
        logger.bchdbg("Orphan tx " + Util.formatHash(tx.getHash()) +
                      ", waiting on " +
                      Util.formatHash(new Buffer(err.missingTxHash, 'base64')));
      } else {
        logger.warn("Rejected tx " +
                    Util.formatHash(tx.getHash()) + ": " +
                    err.message);
        logger.bchdbg(err.stack ? err.stack : err);
      }
    } else {
    }
  });
};

Node.prototype.handleGetdata = function (e) {
  var self = this;

  if (e.message.invs.length == 1) {
    logger.info("Received getdata for " +
                ((e.message.invs[0].type == 1) ? "transaction" : "block") +
                " " + Util.formatHash(e.message.invs[0].hash));
  } else {
    logger.info("Received getdata for " + e.message.invs.length + " objects");
  }

  // inventory to return data for
  var invs = e.message.invs;

  var idx = 0;
  (function next() {

    if (idx >= invs.length) {
      return;
    }

    var inv = invs[idx++];

    switch (inv.type) {
    case 1: // MSG_TX
      var tx = self.txStore.get(inv.hash);
      if (tx) {
        e.conn.sendTx(tx);
      }

      return next();
      break;

    case 2: // MSG_BLOCK
      self.blockChain.getBlockByHash(inv.hash, function (err, block) {
        if (err) {
          logger.warn("Getdata failed, could not load block:\n" +
                      (err.stack ? err.stack : err.toString()));
          return;
        }

        self.storage.getTransactionsByHashes(block.txs, function (err, txs) {
          if (err) {
            logger.warn("Getdata failed, could not load transactions:\n" +
                        (err.stack ? err.stack : err.toString()));
            return;
          }

          e.conn.sendBlock(block, txs);

          // when done sending all the blocks
          if (self.hashContinue && inv.hash.equals(self.hashContinue.getHash())) {
            self.hashContinue = null;
            e.conn.sendInv(self.blockChain.getTopBlock());
          }

          next();
        });
      });
      break;
    }
  })();
};

Node.prototype.handleGetblocks = function (e) {
  var self = this;

  self.blockChain.getBlockByLocator(e.message.starts, function (err, latestCommonBlock) {
    // If there is no common block, quietly fail
    if (!latestCommonBlock) {
      return;
    }

    var nextHeight = latestCommonBlock.height;

    // Array of blocks to send back to client
    // only the inventory hashes will be sent
    var blocks = [];

    // get the next block to send back to user
    (function nextBlock() {
      self.blockChain.getBlockByHeight(++nextHeight, function(err, block) {
        // Quietly fail in case of an error
        if (err) {
          logger.warn("Getblocks failed, could not get block:\n" +
                      (err.stack ? err.stack : err.toString()));
          return;
        }

        // Done gathering blocks because:
        //  - We overran the end of the block chain or
        //  - We hit the limit of 500 or
        //  - We hit the stop hash
        //
        // We also check for block.active as a sanity check.
        if (!block || !block.active ||
            blocks.length === 500 || block.getHash().equals(e.message.stop)) {
          self.hashContinue = blocks[blocks.length - 1];
          e.conn.sendInv(blocks);
          return;
        }

        // Add the block to outgoing
        blocks.push(block);
        nextBlock();
      });
    })();
  });
};

Node.prototype.handleGetheaders = function (e) {
  var self = this;

  self.blockChain.getBlockByLocator(e.message.starts, function (err, latestCommonBlock) {
    if (!latestCommonBlock) {
      return;
    }

    var nextHeight = latestCommonBlock.height;

    // Array of headers to send back to client
    var headers = [];

    // Get the next block
    (function nextBlock() {
      self.blockChain.getBlockByHeight(++nextHeight, function(err, block) {
        // Quietly fail in case of an error
        if (err) {
          logger.warn("Getheaders failed, could not get block:\n" +
                      (err.stack ? err.stack : err.toString()));
          return;
        }

        // Done gathering blocks because:
        //  - We overran the end of the block chain or
        //  - We hit the limit of 500 or
        //  - We hit the stop hash
        //
        // We also check for block.active as a sanity check.
        if (!block || !block.active ||
            headers.length === 2000 || block.getHash().equals(e.message.stop)) {
          e.conn.sendHeaders(headers);
          return;
        }

        // Add the header to outgoing
        headers.push(block.getHeader());
        nextBlock();
      });
    })();
  });
};

/**
 * Broadcast an inv to the network.
 *
 * This function sends an inv message to all active connections.
 */
Node.prototype.sendInv = function (inv) {
  var conns = this.peerManager.getActiveConnections();
  conns.forEach(function (conn) {
    conn.sendInv(inv);
  });
};

/**
 * Broadcast a new transaction to the network.
 *
 * Validates a new tx, adds it to the memory pool and the txSender
 * and broadcasts it across the network.
 */
Node.prototype.sendTx = function (tx, callback) {
  logger.info("Sending tx " + Util.formatHash(tx.getHash()));
  this.txStore.add(tx, (function (err) {
    if (!err) {
      this.txSender.add(tx.getHash().toString('base64'));
      this.sendInv(tx);
    }
    callback(err);
  }).bind(this));
};

/**
 * Get network-adjusted time.
 */
Node.prototype.getAdjustedTime = function () {
  // TODO: Implement actual adjustment
  return Math.floor(new Date().getTime() / 1000);
};

exports.Node = Node