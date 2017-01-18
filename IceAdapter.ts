import options from './options';
import {GPGNetServer} from './GPGNetServer';
import {GPGNetMessage} from './GPGNetMessage';
import {Server as JsonRpcServer} from 'json-rpc2';
import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel } from 'wrtc';
import * as dgram from 'dgram';

export class IceAdapter {
    
    gpgNetServer : GPGNetServer;
    rpcServer : JsonRpcServer;
    tasks : Array<Object>;
    peerConnections : {[key: number]: RTCPeerConnection};
    peerLocalSockets : {[key: number]: dgram.Socket};
    peerLogins : {[key: number]: string};
    gpgNetState : string;

    constructor() {
        this.gpgNetState = 'None';
        this.tasks = new Array<Object>();
        this.gpgNetServer = new GPGNetServer((msg : GPGNetMessage) => {this.onGpgMsg(msg);});
        this.rpcServer = JsonRpcServer.$create();
        this.rpcServer.listenRaw(options.rpc_port, 'localhost');

        this.rpcServer.expose('quit', ()=>{process.exit();});
        this.rpcServer.expose('hostGame', (map : string)=>{this.hostGame(map);});
        this.rpcServer.expose('joinGame', (remotePlayerLogin : string, remotePlayerId : number)=>{this.joinGame(remotePlayerLogin, remotePlayerId);});
        this.rpcServer.expose('connectToPeer', (remotePlayerLogin : string, remotePlayerId : number, offer : boolean)=>{this.connectToPeer(remotePlayerLogin, remotePlayerId, offer);});
        this.rpcServer.expose('disconnectFromPeer', (remotePlayerId : number)=>{this.disconnectFromPeer(remotePlayerId);});
        this.rpcServer.expose('addSdp', (remotePlayerId : number, sdp : string)=>{this.addSdp(remotePlayerId, sdp);});
        this.rpcServer.expose('sendToGpgNet', (header : string, chunks : Array<any>)=>{this.sendToGpgNet(header, chunks);});
        this.rpcServer.expose('status', () : Object =>{return this.status();});
    }

    hostGame(map : string) {
      this.queueGameTask({'type' : 'HostGame',
                          'map' : map});
    }

    joinGame(remotePlayerLogin : string, remotePlayerId : number) {
      this.createPeerRelay(remotePlayerId,
                           remotePlayerLogin,
                           false);
      this.queueGameTask({'type' : 'JoinGame',
                          'remotePlayerLogin' : remotePlayerLogin,
                          'remotePlayerId' : remotePlayerId});
    }

    connectToPeer(remotePlayerLogin : string, remotePlayerId : number, offer : boolean) {
      this.createPeerRelay(remotePlayerId,
                           remotePlayerLogin,
                           offer);
      this.queueGameTask({'type' : 'ConnectToPeer',
                          'remotePlayerLogin' : remotePlayerLogin,
                          'remotePlayerId' : remotePlayerId});
    }
    
    disconnectFromPeer(remotePlayerId : number) {
        if (remotePlayerId in this.peerConnections) {
            delete this.peerConnections[remotePlayerId]; 
        }
        if (remotePlayerId in this.peerLocalSockets) {
            delete this.peerLocalSockets[remotePlayerId]; 
        }
        this.queueGameTask({'type' : 'DisconnectFromPeer',
                            'remotePlayerId' : remotePlayerId});
    }

    addSdp(remotePlayerId : number, sdp : string) {
        if (remotePlayerId in this.peerConnections) {
            delete this.peerConnections[remotePlayerId]; 
        }
    }
    
    sendToGpgNet(header : string, chunks : Array<any>) {
    }
    
    status() : Object {
        let result = {
            'options' : {
                'player_id' : options.id,
                'player_login' : options.login,
                'rpc_port' : options.rpc_port,
                'gpgnet_port' : options.gpgnet_port,
                'lobby_port' : options.lobby_port,
                'stun_server' : options.stun_server,
                'turn_server' : options.turn_server,
                'turn_user' : options.turn_user,
                'turn_pass' : options.turn_pass,
                'log_file' : options.log_file,
            },
            'gpgnet' : {
                'local_port' : this.gpgNetServer.server.address().port,
                'connected' : this.gpgNetServer.socket? true : false,
                'game_state' : this.gpgNetState
            },
            'relays' : []
        };
        
        
        for (let peerId in this.peerConnections) {
            let relayInfo = {
                'remote_player_id' : peerId,
                'remote_player_login' : this.peerLogins[peerId],
                'local_game_udp_port' : this.peerLocalSockets[peerId].address().port,
                'ice_agent' : {
                    'state' : 'implementme',
                    'peer_connected_to_me' : false,
                    'connected_to_peer' : false,
                    'local_candidate' : 'implementme',
                    'remote_candidate' : 'implementme',
                    'remote_sdp' : 'implementme',
                    'time_to_connected' : 1e10,
                }
            };
            result['relays'].push(relayInfo);
        }
        
        return result;
    }
    
    createPeerRelay(remotePlayerId : number,
                    remotePlayerLogin : string,
                    offer : boolean)
    {
        this.peerLogins[remotePlayerId] = remotePlayerLogin;
        let iceServer = {urls: [`stun:${options.stun_server}`, `turn:${options.stun_server}`]};
        if (options.turn_user != '')
        {
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
        dataChannel.onopen = ()=> {
            console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): data channel open`);
            dataChannel.onmessage = (event) => {
                let data = event.data;
                console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): received ${data}`);
                this.peerLocalSockets[remotePlayerId].send(data, options.lobby_port, 'localhost');  
            }
        }
        
        if (offer) {
            console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): create offer`);
            pc.createOffer((desc : RTCSessionDescription) => {
                console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): offer ${desc}`);
            }, (error) => {
                console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): createOffer error ${error}`);
            });
        }
        
        let socket = dgram.createSocket('udp4');
        this.peerLocalSockets[remotePlayerId] = socket;
        socket.on('listening', () => {
            console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): UDP socket listening`);
        });
        socket.bind(0);
        socket.on('message', (msg, rinfo) => {
            console.log(`Relay for ${remotePlayerLogin}(${remotePlayerId}): got ${msg} from ${rinfo.address}:${rinfo.port}`);
            dataChannel.send(msg);
        });
    }
    
    queueGameTask(task : Object) {
        console.log(`queueing task ${task}`);
        this.tasks.push(task);
        this.tryExecuteGameTasks();
    }
    
    tryExecuteGameTasks() {
        if (!this.gpgNetServer.socket)
        {
            return;
        }
        while(this.tasks.length > 0)
        {     
            switch (this.tasks[0]['type'])
            {
                case 'JoinGame':
                case 'ConnectToPeer':
                    if (this.gpgNetState != "Lobby")
                    {
                        return;
                    }
                    if (!(this.tasks[0]['remotePlayerId'] in this.peerLocalSockets)) {
                        console.error(`tryExecuteGameTasks(${this.tasks[0]['type']}): no local UDP socket found for this.tasks[0].remotePlayerId`);
                        return;
                    }
                    let socket = this.peerLocalSockets[this.tasks[0]['remotePlayerId']];
                    this.gpgNetServer.send(new GPGNetMessage(this.tasks[0]['type'], ['127.0.0.1:' + socket.address().port, this.tasks[0]['remoteLogin'], this.tasks[0]['remoteId']]));
                    break;
                case 'HostGame':
                    if (this.gpgNetState != "Lobby")
                    {
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
    
    onGpgMsg(msg : GPGNetMessage) {
        console.log(this);
        switch(msg.header) {
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
    }
    
    rpcNotify(method : string, params : Array<any>) {
        console.error("implement rpcNotify")
    }
}