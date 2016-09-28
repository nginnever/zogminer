const logger = require('../../utils/logger')
const Step = require('step')
const Storage = require('../../storage').Storage

const mongo = require('mongodb')

const Db = mongo.Db
const Server = mongo.Server
const Collection = mongo.Collection

const url = require('url')

const PlainBlock = require('../../schema/block').Block
var PlainTransaction = require('../../schema/transaction').Transaction;

var MongoStorage = exports.MongoStorage = exports.Storage =
function MongoStorage(uri)
{
  var connInfo = url.parse(uri);
  var host = connInfo.host || 'localhost';
  var port = connInfo.port || 27017;
  var dbname = (connInfo.pathname || '/bitcoin').slice(1);

  var db = new Db(dbname, new Server(host, port, {}), {});

  var cBlock;
  var cTransaction;

  function serializeBlock(block) {
    var data = {
      _id: block.getHash(),
      prev_hash: block.prev_hash,
      merkle_root: block.merkle_root,
      timestamp: block.timestamp,
      bits: block.bits,
      nonce: block.nonce,
      version: block.version,
      height: block.height,
      size: block.size,
      active: block.active,
      chainWork: block.chainWork,
      txs: block.txs
    };

    return data;
  };

  function deserializeBlock(data) {
    data.hash = data._id.buffer;
    data.prev_hash = data.prev_hash.buffer;
    data.merkle_root = data.merkle_root.buffer;
    data.chainWork = data.chainWork.buffer;
    data.txs = data.txs.forEach(function (tx) {
      return tx.buffer;
    });
    return new PlainBlock(data);
  };

  function serializeTransaction(tx) {
    var data = {
      _id: tx.getHash(),
      version: tx.version,
      ins: tx.ins.map(function (txin) {
        return {
          s: txin.s,
          q: txin.q,
          o: txin.o
        };
      }),
      outs: tx.outs.map(function (txout) {
        return {
          v: txout.v,
          s: txout.s
        };
      }),
      lock_time: tx.lock_time
    };

    if (tx.affects) {
      data.affects = tx.affects;
    }

    return data;
  };

  function deserializeTransaction(data) {
    data.hash = data._id.buffer;
    data.ins = data.ins ? data.ins.map(function (txin) {
      txin.s = txin.s.buffer;
      txin.o = txin.o.buffer;
      return txin;
    }) : [];
    data.outs = data.outs ? data.outs.map(function (txout) {
      txout.v = txout.v.buffer;
      txout.s = txout.s.buffer;
      return txout;
    }) : [];

    if (data.affects) {
      data.affects = data.affects.map(function (hash) {
        return hash.buffer;
      });
    }

    return new PlainTransaction(data);
  };

  var connected = false;
  this.connect = function (callback) {
    // Only run if not yet connected
    if (connected) {
      callback(null);
      return;
    }
    connected = true;

    logger.info("Connecting to MongoDB ("+uri+")");

    Step(
      function openDatabaseStep() {
        db.open(this);
      },
      function createCollectionsStep(err) {
        if (err) throw err;
        cBlock = new Collection(db, 'blocks');
        cTransaction = new Collection(db, 'transactions');

        this(null);
      },
      function createPrevIndexStep(err) {
        if (err) throw err;
        cBlock.ensureIndex('prev_hash', this);
      },
      function createHeightIndexStep(err) {
        if (err) throw err;
        cBlock.ensureIndex('height', this);
      },
      function createTxsIndexStep(err) {
        if (err) throw err;
        cBlock.ensureIndex('txs', this);
      },
      function createAffectsIndexStep(err) {
        if (err) throw err;
        cTransaction.ensureIndex('affects', this);
      },
      function createOutpointIndexStep(err) {
        if (err) throw err;
        cTransaction.ensureIndex({"ins.o": 1}, this);
      },
      callback
    );
  };

  this.emptyDatabase = function (callback) {
    logger.info('Resetting database');
    Step(
      function dropBlocks() {
        cBlock.remove({}, this);
      },
      function dropTransactions(err) {
        if (err && err.message !== "ns not found") throw err;

        cTransaction.remove({}, this);
      },
      function finish(err) {
        if ("function" === typeof callback) {
          if (err && err.message !== "ns not found") {
            callback(err);
          } else {
            callback(null);
          }
        }
      }
    );
  };

  this.dropDatabase = function (callback) {
    logger.info('Deleting database');

    db.open(function (err) {
      if (err) {
        callback(err);
        return;
      }

      db.dropDatabase(callback);
    });
  };

  this.startTransaction = function (callback) {
    callback(null);
  };

  this.endTransaction = function (callback) {
    callback(null);
  };

  this.saveBlock = function (block, callback) {
    block = serializeBlock(block);
    cBlock.insert(block, {safe: true}, function (err) {
      try {
        // If the block exists already, we want to overwrite it.
        //
        // We could have done an upsert to begin with, but it would
        // have been slower.
        if (err && err.message.indexOf("E11000") !== -1) {
          cBlock.update(
            {_id: block._id}, block, {upsert: true}, function (err) {
              try {
                callback(err);
              } catch (err) {
                logger.error('Storage: Uncaught callback error: ' +
                             (err.stack ? err.stack : err.toString()));
              }
            });
        } else if (err) {
          if ("function" === typeof callback) {
            callback(err);
          }
          return;
        } else {
          // Success
          callback(null);
        }
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  this.saveTransaction = function (tx, callback) {
    // TODO: Overwrite-if-exists
    tx = serializeTransaction(tx);
    cTransaction.insert(tx, function (err) {
      try {
        // It's faster to just ignore the duplicate key error than to
        // check beforehand
        if (err && err.message.indexOf("E11000") == -1) {
          logger.error(err);
          callback(err);
          return;
        }

        callback(null);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  this.saveTransactions = function (txs, callback) {
    // TODO: Overwrite-if-exists
    txs = txs.map(function (tx) {
      return serializeTransaction(tx);
    });
    cTransaction.insert(
      txs,
      {keepGoing: true, safe: true},
      function (err) {
        if (err) {
          if (err.message.indexOf("E11000") != -1) {
            // A transaction we tried to insert already exists. There are
            // several of these in the block chain, e.g.:
            // http://blockexplorer.com/b/91842
            // http://blockexplorer.com/b/91880
            //
            // In MongoDB 1.9.1 there is a flag telling bulk inserts to keep
            // going, but we want to support older MongoDB versions as well, so
            // we need a workaround.
            //
            // The workaround is to just fall back to inserting each transaction
            // individually. Here we go.
            Step(
              function insertIndividual() {
                var parallel = this.parallel;
                txs.forEach(function (tx) {
                  var callback = parallel();
                  cTransaction.update({_id: tx._id}, tx, {upsert: true}, function (err) {
                    // Only pass on errors other than duplicate key
                    if (err && err.message.indexOf("E11000") == -1) {
                      callback(err);
                    } else callback();
                  });
                });
              },
              // After all the parallel insertions have finished, we go back to
              // the normal callback that would have been run normally. This won't
              // cause an endless loop, because we ignored the duplicate key
              // error.
              arguments.callee
            );
            return;
          } else {
            callback(err);
            return;
          }
        }

        callback(null);
      }
    );
  };

  // TODO: Currently MongoDB creates an index of spent outpoints by
  //       indexing the txin.outpoint fields in transactions.
  //
  //       This does not, however, cover memory transactions well.
  //
  //       In the future we will need to have something closer to the
  //       original client's ConnectInputs mechanism.
  var connectTransaction = this.connectTransaction =
  function connectTransaction(tx, callback) {
    connectTransactions([tx], callback);
  };

  var connectTransactions = this.connectTransactions =
  function connectTransactions(txs, callback) {
    callback(null);
  };

  var disconnectTransaction = this.disconnectTransaction =
  function disconnectTransaction(tx, callback) {
    disconnectTransactions([tx], callback);
  };

  var disconnectTransactions = this.disconnectTransactions =
  function disconnectTransactions(txs, callback) {
    callback(null);
  };

  var getTransactionByHash = this.getTransactionByHash =
  function getTransactionByHash(hash, callback) {
    cTransaction.findOne({_id: hash}, function (err, result) {
      try {
        if (result) {
          result = deserializeTransaction(result);
        } else {
          // return undefined
          result = undefined;
        }
        callback(err, result);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  this.getTransactionsByHashes = function (hashes, callback) {
    cTransaction.find({_id: {$in: hashes}}, function (err, results) {
      results.toArray(function (err, results) {
        try {
          if (err) {
            callback(err);
            return;
          }
          if (results.length) {
            results = results.map(function (tx) {
              return deserializeTransaction(tx);
            });
          }
          callback(null, results);
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      });
    });
  };

  this.getOutputsByHashes = function (hashes, callback) {
    cTransaction.find({_id: {$in: hashes}}, {fields: ["_id", "outs"]}, function (err, results) {
      results.toArray(function (err, results) {
        try {
          if (err) {
            callback(err);
            return;
          }
          if (results.length) {
            results = results.map(function (tx) {
              return deserializeTransaction(tx);
            });
          }
          callback(null, results);
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      });
    });
  };

  this.getBlocksByHeights = function (heights, callback) {
    cBlock.find({height: {$in: heights}}, function (err, results) {
      results.toArray(function (err, results) {
        try {
          if (err) {
            callback(err);
            return;
          }
          results = results.map(function (block) {
            return deserializeBlock(block);
          });
          callback(err, results);
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      });
    });
  };

  var getBlockByHash = this.getBlockByHash =
  function getBlockByHash(hash, callback) {
    cBlock.findOne({_id: hash}, function (err, result) {
      try {
        if (err) {
          callback(err);
          return;
        }
        if (result) {
          result = deserializeBlock(result);
        } else {
          // return undefined
          result = undefined;
        }
        callback(err, result);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  var getBlocksByHashes = this.getBlocksByHashes =
  function getBlocksByHashes(hashes, callback) {
    cBlock.find({_id: {$in: hashes}}, function (err, results) {
      results.toArray(function (err, results) {
        try {
          if (err) {
            callback(err);
            return;
          }

          if (results) {
            results = results.map(function (block) {
              return deserializeBlock(block);
            });
          }
          callback(null, results);
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      });
    });
  };

  var getBlockByHeight = this.getBlockByHeight =
  function getBlockByHeight(height, callback) {
    cBlock.findOne({height: height, active: true}, function (err, result) {
      try {
        if (result) {
          result = deserializeBlock(result);
        }
        callback(err, result);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  var getBlockByPrev = this.getBlockByPrev =
  function getBlockByPrev(block, callback) {
    if ("object" == typeof block && block.hash) {
      block = block.hash;
    }
    cBlock.findOne({prev_hash: block}, function (err, block) {
      try {
        if (err) {
          callback(err);
          return;
        }

        if (block) {
          block = deserializeBlock(block);
        }

        callback(err, block);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  var getTopBlock = this.getTopBlock =
  function getTopBlock(callback) {
    cBlock
      .find({active: true})
      .sort('height', -1)
      .limit(1)
      .toArray(function (err, result) {
        if (err) {
          callback(err);
          return;
        }

        if (result.length > 0) {
          callback(null, deserializeBlock(result[0]));
        } else {
          callback(null, null);
        }
      });
  };

  var getBlockSlice = this.getBlockSlice =
  function getBlockSlice(start, limit, callback) {
    if (start >= 0) {
      var query = cBlock.find({active: true, height:{'$gte': start}}, ["_id"]).sort('height', 1);

      if (limit) {
        query.limit(limit);
      }

      query.toArray(function (err, results) {
        if (err) {
          callback(err);
          return;
        }

        results = results.map(function (data) {
          return data._id.buffer;
        });

        callback(null, results);
      });
    } else {
      getTopBlock(function (err, topBlock) {
        if (err) {
          callback(err);
          return;
        }
        var chainHeight = topBlock.height;
        start = chainHeight + start;

        if (start < 0) {
          start = 0;
        }

        getBlockSlice(start, limit, callback);
      });
    }
  };

  /**
   * Find the latest matching block from a locator.
   *
   * A locator is basically just a list of hashes. We send it to the database
   * and ask it to get the latest block that is in the list.
   */
  var getBlockByLocator = this.getBlockByLocator =
  function (locator, callback) {
    cBlock
      .find({_id: {"$in": locator}, active: true})
      .sort('height', -1)
      .limit(1)
      .toArray(function (err, result) {
        try {
          if (err) {
            callback(err);
            return;
          }

          if (result.length > 0) {
            callback(null, deserializeBlock(result[0]));
          } else {
            callback(null, null);
          }
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      })
    ;
  };

  var countConflictingTransactions = this.countConflictingTransactions =
  function countConflictingTransactions(outpoints, callback) {
    // List of queries that will search for other transactions spending
    // the same outs.
    var srcOutCondList = outpoints.map(function (outpoint) {
      return {'ins.o': outpoint};
    });
    cTransaction.find({"$or": srcOutCondList}).count(function (err, result) {
      try {
        callback(err, result);
      } catch (err) {
        logger.error('Storage: Uncaught callback error: ' +
                     (err.stack ? err.stack : err.toString()));
      }
    });
  };

  var getConflictingTransactions = this.getConflictingTransactions =
  function getConflictingTransactions(outpoints, callback) {
    var srcOutCondList = outpoints.map(function (outpoint) {
      return {'ins.o': outpoint};
    });
    cTransaction.find({"$or": srcOutCondList}, function (err, results) {
      results.toArray(function (err, results) {
        try {
          if (err) {
            callback(err);
            return;
          }

          if (results.length > 0) {
            results = results.map(function (tx) {
              return deserializeTransaction(tx);
            });
          }
          callback(null, results);
        } catch (err) {
          logger.error('Storage: Uncaught callback error: ' +
                       (err.stack ? err.stack : err.toString()));
        }
      });
    });
  };

  var knowsBlock = this.knowsBlock =
  function knowsBlock(hash, callback) {
    cBlock.find({'_id': hash}).count(function (err, count) {
      callback(err, !!count);
    });
  };

  var knowsTransaction = this.knowsTransaction =
  function knowsTransction(hash, callback) {
    cTransaction.find({'_id': hash}).count(function (err, count) {
      callback(err, !!count);
    });
  };

  var getContainingBlock = this.getContainingBlock =
  function getContainingBlock(txHash, callback) {
    cBlock.findOne({'txs': txHash}, function (err, block) {
      callback(err, block._id);
    });
  };

  var getAffectedTransactions = this.getAffectedTransactions =
  function getAffectedTransactions(addrHashes, callback) {
    if (Buffer.isBuffer(addrHashes)) {
      addrHashes = [addrHashes];
    } else if (Array.isArray(addrHashes)) {
      // No change needed
    } else {
      throw new Error('Invalid addrHashes parameter, expected '+
                      'Buffer or array of buffers.');
    }

    cTransaction.find({'affects':{'$in':addrHashes}}, function (err, results) {
      results.toArray(function (err, txs) {
        if (err) throw err;
        var hashes = [];
        for (var i = 0; i < txs.length; i++) {
          hashes.push(txs[i]._id);
        }
        callback(null, hashes);
      })
    });
  };
};

util.inherits(MongoStorage, Storage)