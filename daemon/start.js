#!/usr/bin/env node

const createNode = require('./init').createNode

var node = createNode({ welcome: true });
node.start();