import options from './options';
import { GPGNetServer } from './GPGNetServer';
import { GPGNetMessage } from './GPGNetMessage';
import { server as JsonRpcServer } from 'jayson';
import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel } from 'wrtc';
import * as dgram from 'dgram';
import { Server as TCPServer, Socket as TCPSocket } from 'net';
import * as winston from 'winston';

export class IceAdapter {

  gpgNetServer: GPGNetServer;
  rpcServer: JsonRpcServer;
  rpcServerRaw: TCPServer;
  rpcSocket: TCPSocket;
  tasks: Array<Object>;
  peerConnections: { [key: number]: RTCPeerConnection };
  peerLocalSockets: { [key: number]: dgram.Socket };
  peerLogins: { [key: number]: string };
  gpgNetState: string;

  constructor() {
    this.gpgNetState = 'None';
    this.tasks = new Array<Object>();
    this.peerConnections = {};
    this.peerLocalSockets = {};
    this.peerLogins = {};
    this.gpgNetServer = new GPGNetServer((msg: GPGNetMessage) => { this.onGpgMsg(msg); });
    this.rpcServer = JsonRpcServer({
      'quit': (args, callback) => { process.exit(); },
      'hostGame': (args, callback) => { this.hostGame(args[0]); },
      'joinGame': (args, callback) => { this.joinGame(args[0], args[1]); },
      'connectToPeer': (args, callback) => { this.connectToPeer(args[0], args[1], args[2]); },
      'disconnectFromPeer': (args, callback) => { this.disconnectFromPeer(args[0]); },
      'addSdp': (args, callback) => { this.addSdp(args[0], args[1]); },
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
    if (remotePlayerId in this.peerConnections) {
      delete this.peerConnections[remotePlayerId];
    }
    if (remotePlayerId in this.peerLocalSockets) {
      delete this.peerLocalSockets[remotePlayerId];
    }
    this.queueGameTask({
      'type': 'DisconnectFromPeer',
      'remotePlayerId': remotePlayerId
    });
  }

  addSdp(remotePlayerId: number, sdp: string) {
    if (remotePlayerId in this.peerConnections) {
      delete this.peerConnections[remotePlayerId];
    }
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


    for (let peerId in this.peerConnections) {
      let relayInfo = {
        'remote_player_id': peerId,
        'remote_player_login': this.peerLogins[peerId],
        'local_game_udp_port': this.peerLocalSockets[peerId].address().port,
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

  createPeerRelay(remotePlayerId: number,
    remotePlayerLogin: string,
    offer: boolean) {
    this.peerLogins[remotePlayerId] = remotePlayerLogin;
    let iceServer = { urls: [`stun:${options.stun_server}`, `turn:${options.stun_server}`] };
    if (options.turn_user != '') {
      iceServer['username'] = options.turn_user;
      iceServer['credential'] = options.turn_pass;
    }
    let pc = new RTCPeerConnection({
      iceServers: [iceServer]
    });
    this.peerConnections[remotePlayerId] = pc;

    pc.onicecandidate = (candidate) => {
      if (candidate.candidate) {
        this.rpcNotify('onSdp', [options.id, remotePlayerId, candidate.candidate]);
      }
    }

    let dataChannel = pc.createDataChannel('faf');
    dataChannel.onopen = () => {
      winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): data channel open`);
      dataChannel.onmessage = (event) => {
        let data = event.data;
        winston.debug(`Relay for ${remotePlayerLogin}(${remotePlayerId}): received ${data}`);
        this.peerLocalSockets[remotePlayerId].send(data, options.lobby_port, 'localhost');
      }
    }

    if (offer) {
      winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): create offer`);
      pc.createOffer((desc: RTCSessionDescription) => {
        winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): offer ${desc}`);
      }, (error) => {
        winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): createOffer error ${error}`);
      });
    }

    let socket = dgram.createSocket('udp4');
    this.peerLocalSockets[remotePlayerId] = socket;
    socket.on('listening', () => {
      winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): UDP socket listening`);
    });
    socket.bind(0);
    socket.on('message', (msg, rinfo) => {
      winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): got ${msg} from ${rinfo.address}:${rinfo.port}`);
      dataChannel.send(msg);
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
      switch (this.tasks[0]['type']) {
        case 'JoinGame':
        case 'ConnectToPeer':
          if (this.gpgNetState != "Lobby") {
            return;
          }
          if (!(this.tasks[0]['remotePlayerId'] in this.peerLocalSockets)) {
            winston.error(`tryExecuteGameTasks(${this.tasks[0]['type']}): no local UDP socket found for this.tasks[0].remotePlayerId`);
            return;
          }
          let socket = this.peerLocalSockets[this.tasks[0]['remotePlayerId']];
          this.gpgNetServer.send(new GPGNetMessage(this.tasks[0]['type'], ['127.0.0.1:' + socket.address().port, this.tasks[0]['remoteLogin'], this.tasks[0]['remoteId']]));
          break;
        case 'HostGame':
          if (this.gpgNetState != "Lobby") {
            return;
          }
          this.gpgNetServer.send(new GPGNetMessage('HostGame', [this.tasks[0]['map']]));
          break;
        case 'DisconnectFromPeer':
          this.gpgNetServer.send(new GPGNetMessage('DisconnectFromPeer', [this.tasks[0]['remoteId']]));
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
