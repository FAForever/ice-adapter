import * as yargs from "yargs";

export let options = yargs
  .usage('Usage: $0 --id [num] --login [string]')
  .demand(['id', 'login'])
  .describe('id', 'set the ID of the local player')
  .describe('login', 'set the login of the local player, e.g. "Rhiza"')
  .default('rpc_port', 7236)
  .describe('rpc_port', 'set the port of internal JSON-RPC server')
  .default('gpgnet_port', 7237)
  .describe('gpgnet_port', 'set the port of internal GPGNet server')
  .default('lobby_port', 7238)
  .describe('lobby_port', 'set the port the game lobby should use for incoming UDP packets from the PeerRelay')
  .default('stun_server', 'test.faforever.com')
  .describe('stun_server', 'set the STUN server hostname')
  .default('turn_server', 'test.faforever.com')
  .describe('turn_server', 'set the TURN server hostname')
  .default('turn_user', 'test')
  .describe('turn_user', 'set the TURN username')
  .default('turn_pass', 'test')
  .describe('turn_pass', 'set the TURN password')
  .default('log_file', '')
  .describe('log_file', 'set a verbose log file')
  .argv;

export default options;

