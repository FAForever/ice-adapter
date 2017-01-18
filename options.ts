import * as optimist from 'optimist';

export let options = optimist
    .usage('Usage: $0 --id [num] --login [string]')
    .demand(['id','login'])
    .describe('id', 'set the ID of the local player')
    .describe('login', 'set the login of the local player, e.g. "Rhiza"')
    .default('rpc_port', 7236)
    .describe('rpc_port', 'set the port of internal JSON-RPC server')
    .default('ice_port_min', -1)
    .describe('ice_port_min', 'start of port range to use for ICE local host candidates')
    .default('ice_port_max', -1)
    .describe('ice_port_min', 'end of port range to use for ICE local host candidates')
    .default('upnp', true)
    .describe('upnp', 'use UPNP for NAT router port configuration')
    .default('gpgnet_port', 7237)
    .describe('gpgnet_port', 'set the port of internal GPGNet server')
    .default('lobby_port', 7238)
    .describe('lobby_port', 'set the port the game lobby should use for incoming UDP packets from the PeerRelay')
    .default('stun_ip', '37.58.123.3')
    .describe('stun_ip', 'set the STUN IP address')
    .default('turn_ip', '37.58.123.3')
    .describe('turn_ip', 'set the TURN IP address')
    .default('turn_user', '')
    .describe('turn_user', 'set the TURN username')
    .default('turn_pass', '')
    .describe('turn_pass', 'set the TURN password')
    .default('log_file', '')
    .describe('log_file', 'set a verbose log file')
    .argv;

export default options;

