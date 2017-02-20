import * as yargs from "yargs";

export let options = yargs
  .usage('Usage: $0 --id [num] --login [string]')
  .demand(['id', 'login'])
  .describe('id', 'set the ID of the local player')
  .describe('login', 'set the login of the local player, e.g. "Rhiza"')
  .default('rpc-port', 7236)
  .describe('rpc-port', 'set the port of internal JSON-RPC server')
  .alias('rpc-port', 'rpc_port')
  .default('gpgnet-port', 7237)
  .describe('gpgnet-port', 'set the port of internal GPGNet server')
  .alias('gpgnet-port', 'gpgnet_port')
  .default('lobby-port', 7238)
  .describe('lobby-port', 'set the port the game lobby should use for incoming UDP packets from the PeerRelay')
  .alias('lobby-port', 'lobby_port')
  .default('log-file', '')
  .describe('log-file', 'set a verbose log file')
  .alias('log-file', 'log_file')
  .argv;

export default options;

