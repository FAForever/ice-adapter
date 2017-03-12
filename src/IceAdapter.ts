import { server as JsonRpcServer } from 'jayson';
import options from './options';
import { GPGNetServer } from './GPGNetServer';
import { GPGNetMessage } from './GPGNetMessage';
import { PeerRelay } from './PeerRelay';
import { Server as TCPServer, Socket as TCPSocket } from 'net';
import logger from './logger';

export class IceAdapter {

  gpgNetServer: GPGNetServer;
  rpcServer: JsonRpcServer;
  rpcServerRaw: TCPServer;
  rpcSocket: TCPSocket;
  tasks: Array<Object>;
  peerRelays: { [key: number]: PeerRelay };
  gpgNetState: string;
  version: string;
  gametaskString: string;
  iceServers: Array<Object>;

  constructor() {
    this.gpgNetState = 'None';
    this.tasks = new Array<Object>();
    this.peerRelays = {};
    this.gpgNetServer = new GPGNetServer((msg: GPGNetMessage) => { this.onGpgMsg(msg); });
    this.gpgNetServer.on('connected', () => {
      this.rpcNotify('onConnectionStateChanged', ['Connected']);
    });
    this.gpgNetServer.on('disconnected', () => {
      //disconnect all peers when the game exits
      for (let peerId in this.peerRelays) {
        this.peerRelays[peerId].close();
        delete this.peerRelays[peerId];
      }
      this.peerRelays = {};
      this.gametaskString = 'Idle';
      this.gpgNetState = 'None';

      this.rpcNotify('onConnectionStateChanged', ['Disconnected']);
    });

    this.version = require('../package.json').version;
    this.gametaskString = 'Idle';

    this.initRpcServer();
    this.iceServers = new Array<Object>();
  }

  initRpcServer() {
    this.rpcServer = JsonRpcServer({
      'quit': (args, callback) => { throw ("Exit"); },
      'hostGame': (args, callback) => { this.hostGame(args[0]); },
      'joinGame': (args, callback) => { this.joinGame(args[0], args[1]); },
      'connectToPeer': (args, callback) => { this.connectToPeer(args[0], args[1], args[2]); },
      'disconnectFromPeer': (args, callback) => { this.disconnectFromPeer(args[0]); },
      'iceMsg': (args, callback) => { this.iceMsg(args[0], args[1]); },
      'sendToGpgNet': (args, callback) => { this.sendToGpgNet(args[0], args[1]); },
      'setIceServers': (args, callback) => { this.setIceServers(args[0]); },
      'status': (args, callback) => { callback(null, this.status()); }
    })
    this.rpcServerRaw = this.rpcServer.tcp();
    this.rpcServerRaw.listen(options.rpcPort, 'localhost');;
    this.rpcServerRaw.on('listening', () => {
      logger.info(`JSONRPC server listening on port ${this.rpcServerRaw.address().port}`);
    });
    this.rpcServerRaw.on('connection', (s) => {
      this.rpcSocket = s;
      s.on('close', () => {
        if (s == this.rpcSocket) {
          logger.info(`JSONRPC server client disconnected`);
          delete this.rpcSocket;
        }
      });
    });
  }

  hostGame(map: string) {
    this.queueGameTask({
      type: 'HostGame',
      map: map
    });
    this.gametaskString = `Hosting map ${map}.`;
  }

  joinGame(remotePlayerLogin: string, remotePlayerId: number) {
    let relay = this.createPeerRelay(remotePlayerId,
      remotePlayerLogin,
      false);
    relay.on('localSocketListening', () => {
      this.queueGameTask({
        type: 'JoinGame',
        remotePlayerLogin: remotePlayerLogin,
        remotePlayerId: remotePlayerId
      });
    });
    this.gametaskString = `Joining game from player ${remotePlayerLogin}.`;
  }

  connectToPeer(remotePlayerLogin: string, remotePlayerId: number, offer: boolean) {
    let relay = this.createPeerRelay(remotePlayerId,
      remotePlayerLogin,
      offer);
    relay.on('localSocketListening', () => {
      this.queueGameTask({
        type: 'ConnectToPeer',
        remotePlayerLogin: remotePlayerLogin,
        remotePlayerId: remotePlayerId
      });
    });
  }

  disconnectFromPeer(remotePlayerId: number) {
    this.queueGameTask({
      type: 'DisconnectFromPeer',
      remotePlayerId: remotePlayerId
    });
    if (remotePlayerId in this.peerRelays) {
      this.peerRelays[remotePlayerId].close();
      delete this.peerRelays[remotePlayerId];
    }
  }

  iceMsg(remotePlayerId: number, msg: any) {
    if (!(remotePlayerId in this.peerRelays)) {
      logger.error(`iceMsg: remotePlayerId ${remotePlayerId} not in this.peerRelays`);
      return;
    }
    this.peerRelays[remotePlayerId].addIceMsg(msg);
  }

  sendToGpgNet(header: string, chunks: Array<any>) {
    this.gpgNetServer.send(new GPGNetMessage(header, chunks));
  }

  setIceServers(iceServers: Array<Object>) {
    this.iceServers = iceServers;
    for (let peerId in this.peerRelays) {
      let relay = this.peerRelays[peerId];
      relay.setIceServers(iceServers);
    }
    logger.debug(`setIceServers called with ${JSON.stringify(iceServers)}`);
  }

  status(): Object {
    let result = {
      'version': this.version,
      'options': {
        'player_id': options.id,
        'player_login': options.login,
        'rpc_port': options.rpcPort,
        'gpgnet_port': options.gpgnetPort,
        'lobby_port': options.lobbyPort,
        'log_file': options.logFile,
      },
      'gpgnet': {
        'local_port': this.gpgNetServer.server.address().port,
        'connected': this.gpgNetServer.socket ? true : false,
        'game_state': this.gpgNetState,
        'task_string': this.gametaskString
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
          'state': relay.iceConnectionState,
          'datachannel_open': relay.dataChannelIsOpen,
          'loc_cand_addr': relay.localCandAddress,
          'rem_cand_addr': relay.remoteCandAddress,
          'loc_cand_type': relay.localCandType,
          'rem_cand_type': relay.remoteCandType,
          'time_to_connected': relay.connectedTime ? relay.connectedTime[1] / 1e9 : -1,
        }
      };
      result['relays'].push(relayInfo);
    }

    return result;
  }

  createPeerRelay(remotePlayerId: number, remotePlayerLogin: string, offer: boolean): PeerRelay {

    if (this.iceServers.length == 0) {
      logger.error(`no ICE servers while creating PeerRelay for remot player ${remotePlayerLogin}(${remotePlayerId}). Call setIceServers up front from client. See https://developer.mozilla.org/en-US/docs/Web/API/RTCConfiguration`);
    }

    let relay = new PeerRelay(remotePlayerId, remotePlayerLogin, offer, this.iceServers);
    this.peerRelays[remotePlayerId] = relay;

    relay.on('iceMessage', (msg) => {
      this.rpcNotify('onIceMsg', [options.id, remotePlayerId, msg]);
    });

    relay.on('iceConnectionStateChanged', (state) => {
      this.rpcNotify('onIceConnectionStateChanged', [options.id, remotePlayerId, state]);
    });

    relay.on('datachannelOpen', () => {
      this.rpcNotify('onDatachannelOpen', [options.id, remotePlayerId]);
    });

    return relay;
  }

  queueGameTask(task: Object) {
    logger.debug(`queueing task ${JSON.stringify(task)}`);
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
            logger.error(`tryExecuteGameTasks(${task['type']}): no local UDP socket found for ${remoteId}`);
            return;
          }
          if (!(this.peerRelays[task['remotePlayerId']].localPort)) {
            logger.error(`tryExecuteGameTasks(${task['type']}): no local UDP socket opened for ${remoteId}`);
            return;
          }
          let port = this.peerRelays[task['remotePlayerId']].localPort;
          this.gpgNetServer.send(new GPGNetMessage(task['type'], ['127.0.0.1:' + port, task['remotePlayerLogin'], remoteId]));
          break;
        case 'HostGame':
          if (this.gpgNetState != "Lobby") {
            return;
          }
          this.gpgNetServer.send(new GPGNetMessage('HostGame', [task['map']]));
          break;
        case 'DisconnectFromPeer':
          this.gpgNetServer.send(new GPGNetMessage('DisconnectFromPeer', [task['remotePlayerId']]));
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
            options.lobbyPort,
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
      logger.debug(`sending notification to client: ${data}`);
      this.rpcSocket.write(data);
    }
  }
}
