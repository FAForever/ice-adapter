import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel } from 'wrtc';
import * as dgram from 'dgram';
import options from './options';
import * as winston from 'winston';

class PeerRelay {
  peerConnection: RTCPeerConnection;
  localSocket: dgram.Socket;
  localAddress: Object;
  dataChannel: RTCDataChannel;
  constructor(public remoteId: number,
    public remoteLogin: string,
    public createOffer: boolean) {

    let iceServer = { urls: [`stun:${options.stun_server}`, `turn:${options.stun_server}`] };
    if (options.turn_user != '') {
      iceServer['username'] = options.turn_user;
      iceServer['credential'] = options.turn_pass;
    }

    this.peerConnection = new RTCPeerConnection({
      iceServers: [iceServer]
    });
    this.localSocket = dgram.createSocket('udp4');
    this.localSocket.on('listening', () => {
      this.localAddress = this.localSocket.address();
      console.log("PeerRelay for ${this.login} listening on ${address.address}:${address.port}");
    });
    this.localSocket.bind(0);

    this.dataChannel = this.peerConnection.createDataChannel('faf');
    this.dataChannel.onopen = () => {
      winston.info(`Relay for ${remotePlayerLogin}(${remotePlayerId}): data channel open`);
      this.dataChannel.onmessage = (event) => {
        var data = event.data;
        console.log("PeerRelay for ${this.login} received ${event.data}");
        this.localSocket.send(event.data, options.lobby_port, "localhost");
      }
    }


    this.localSocket.on('message', (msg, rinfo) => {
      console.log(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
      this.dataChannel.send(msg);
    });
  }
}
