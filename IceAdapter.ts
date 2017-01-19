import options from './options';
import { GPGNetServer } from './GPGNetServer';
import { GPGNetMessage } from './GPGNetMessage';
import { server as JsonRpcServer } from 'jayson';
import { PeerRelay } from './PeerRelay';
import * as dgram from 'dgram';
import { Server as TCPServer, Socket as TCPSocket } from 'net';
import * as winston from 'winston';

export class IceAdapter {

  gpgNetServer: GPGNetServer;
  rpcServer: JsonRpcServer;
  rpcServerRaw: TCPServer;
  rpcSocket: TCPSocket;
  tasks: Array<Object>;
  peerRelays: { [key: number]: PeerRelay };
  gpgNetState: string;

  constructor() {
    this.gpgNetState = 'None';
    this.tasks = new Array<Object>();
    this.peerRelays = {};
    this.gpgNetServer = new GPGNetServer((msg: GPGNetMessage) => { this.onGpgMsg(msg); });
    this.initRpcServer();
  }

  initRpcServer() {
    this.rpcServer = JsonRpcServer({
      'quit': (args, callback) => { process.exit(); },
      'hostGame': (args, callback) => { this.hostGame(args[0]); },
      'joinGame': (args, callback) => { this.joinGame(args[0], args[1]); },
      'connectToPeer': (args, callback) => { this.connectToPeer(args[0], args[1], args[2]); },
      'disconnectFromPeer': (args, callback) => { this.disconnectFromPeer(args[0]); },
      'iceMsg': (args, callback) => { this.iceMsg(args[0], args[1]); },
      'sendToGpgNet': (args, callback) => { this.sendToGpgNet(args[0], args[1]); },
      'status': (args, callback) => { callback(null, this.status()); }
    })
    this.rpcServerRaw = this.rpcServer.tcp();
    this.rpcServerRaw.listen(options.rpc_port, 'localhost');;
    this.rpcServerRaw.on('listening', () => {
      winston.info(`JSONRPC server listening on port ${this.rpcServerRaw.address().port}`);
    });
    this.rpcServerRaw.on('connection', (s) => {
      this.rpcSocket = s;
      /*
      s.on('data', (data : Buffer) => {
          console.log(`JSONRPC server recv data ${data}`);
      });
      */
      s.on('close', () => {
        if (s == this.rpcSocket) {
          winston.info(`JSONRPC server client disconnected`);
          delete this.rpcSocket;
        }
      });
    });
  }

  hostGame(map: string) {
    this.queueGameTask({
      'type': 'HostGame',
      'map': map
    });
  }

  joinGame(remotePlayerLogin: string, remotePlayerId: number) {
    this.createPeerRelay(remotePlayerId,
      remotePlayerLogin,
      false);
    this.queueGameTask({
      'type': 'JoinGame',
      'remotePlayerLogin': remotePlayerLogin,
      'remotePlayerId': remotePlayerId
    });
  }

  connectToPeer(remotePlayerLogin: string, remotePlayerId: number, offer: boolean) {
    this.createPeerRelay(remotePlayerId,
      remotePlayerLogin,
      offer);
    this.queueGameTask({
      'type': 'ConnectToPeer',
      'remotePlayerLogin': remotePlayerLogin,
      'remotePlayerId': remotePlayerId
    });
  }

  disconnectFromPeer(remotePlayerId: number) {
    if (remotePlayerId in this.peerRelays) {
      delete this.peerRelays[remotePlayerId];
    }
    this.queueGameTask({
      'type': 'DisconnectFromPeer',
      'remotePlayerId': remotePlayerId
    });
  }

  iceMsg(remotePlayerId: number, msg : any) {
  }

  sendToGpgNet(header: string, chunks: Array<any>) {
  }

  status(): Object {
    let result = {
      'options': {
        'player_id': options.id,
        'player_login': options.login,
        'rpc_port': options.rpc_port,
        'gpgnet_port': options.gpgnet_port,
        'lobby_port': options.lobby_port,
        'stun_server': options.stun_server,
        'turn_server': options.turn_server,
        'turn_user': options.turn_user,
        'turn_pass': options.turn_pass,
        'log_file': options.log_file,
      },
      'gpgnet': {
        'local_port': this.gpgNetServer.server.address().port,
        'connected': this.gpgNetServer.socket ? true : false,
        'game_state': this.gpgNetState
      },
      'relays': []
    };


    for (let peerId in this.peerRelays) {
      let relay = this.peerRelays[peerId];
      let relayInfo = {
        'remote_player_id': relay.remoteId,
        'remote_player_login': relay.remoteLogin,
        'local_game_udp_port': relay.localPort,
        'ice_agent': {
          'state': 'implementme',
          'peer_connected_to_me': false,
          'connected_to_peer': false,
          'local_candidate': 'implementme',
          'remote_candidate': 'implementme',
          'remote_sdp': 'implementme',
          'time_to_connected': 1e10,
        }
      };
      result['relays'].push(relayInfo);
    }

    return result;
  }

  createPeerRelay(remotePlayerId: number, remotePlayerLogin: string, offer: boolean) {
    let relay = new PeerRelay(remotePlayerId, remotePlayerLogin, offer);
    this.peerRelays[remotePlayerId] = relay;

    relay.on('iceMessage', (msg) => {
      this.rpcNotify('onIceMsg', [options.id, remotePlayerId, msg]);
    });
  }

  queueGameTask(task: Object) {
    winston.debug(`queueing task ${task}`);
    this.tasks.push(task);
    this.tryExecuteGameTasks();
  }

  tryExecuteGameTasks() {
    if (!this.gpgNetServer.socket) {
      return;
    }
    while (this.tasks.length > 0) {
      let task = this.tasks[0];
      switch (task['type']) {
        case 'JoinGame':
        case 'ConnectToPeer':
          let remoteId = task['remotePlayerId'];
          if (this.gpgNetState != "Lobby") {
            return;
          }
          if (!(remoteId in this.peerRelays)) {
            winston.error(`tryExecuteGameTasks(${task['type']}): no local UDP socket found for ${remoteId}`);
            return;
          }
          if (!(this.peerRelays[task['remotePlayerId']].localPort)) {
            winston.error(`tryExecuteGameTasks(${task['type']}): no local UDP socket opened for ${remoteId}`);
            return;
          }
          let port = this.peerRelays[task['remotePlayerId']].localPort;
          this.gpgNetServer.send(new GPGNetMessage(task['type'], ['127.0.0.1:' + port, task['remoteLogin'], remoteId]));
          break;
        case 'HostGame':
          if (this.gpgNetState != "Lobby") {
            return;
          }
          this.gpgNetServer.send(new GPGNetMessage('HostGame', [task['map']]));
          break;
        case 'DisconnectFromPeer':
          this.gpgNetServer.send(new GPGNetMessage('DisconnectFromPeer', [task['remoteId']]));
          break;
      }
      this.tasks.shift();
    }
  }

  onGpgMsg(msg: GPGNetMessage) {
    switch (msg.header) {
      case 'GameState':
        this.gpgNetState = msg.chunks[0];
        if (this.gpgNetState == 'Idle') {
          this.gpgNetServer.send(new GPGNetMessage('CreateLobby', [0,
            options.lobby_port,
            options.login,
            options.id,
            1]));
        }
        this.tryExecuteGameTasks();
        break;
    }
    this.rpcNotify('onGpgNetMessageReceived', [msg.header, msg.chunks]);
  }

  rpcNotify(method: string, params: Array<any>) {
    if (this.rpcSocket) {
      let data = JSON.stringify({
        jsonrpc: '2.0',
        method: method,
        params: params
      });
      this.rpcSocket.write(data);
    }
  }
}
