'use strict'

const logger = require('./utils/logger')

var Storage = exports.Storage = function Storage()
{

}

Storage.get = function (uri)
{
  var Storage
  var storageProtocol = ""+uri.match(/^[a-z]+/i)
  // hardcode mongodb for now
  storageProtocol = 'mongodb'

  switch (storageProtocol) {
  case 'mongodb':
    try {
      Storage = require('./db/mongo/storage').Storage;
      return new Storage(uri);
    } catch (e) {
      logger.error("MongoDB " + e);
      throw new Error("Could not instantiate MongoDB backend");
    }

  case 'kyoto':
    try {
      Storage = require('./db/kyoto/storage').Storage;
      return new Storage(uri);
    } catch (e) {
      logger.error("KyotoCabinet " + e);
      throw new Error("Could not instantiate KyotoCabinet backend");
    }
    
  case 'leveldb':
    try {
      Storage = require('./db/leveldb/storage').Storage;
      return new Storage(uri);
    } catch (e) {
      logger.error("LevelDB " + e);
      throw new Error("Could not instantiate LevelDB (LevelDOWN) backend");
    }

  case 'levelup':
    try {
      Storage = require('./db/leveldown/storage').Storage;
      return new Storage(uri);
    } catch (e) {
      logger.error("LevelDB " + e);
      throw new Error("Could not instantiate LevelDB (LevelUP) backend");
    }

  default:
    throw new Error('Unknown storage protocol "'+storageProtocol+'"');
  }
}