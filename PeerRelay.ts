import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel } from 'wrtc';
import * as dgram from 'dgram';
import options from './options';

class PeerRelay {
    peerConnection : RTCPeerConnection;
    localSocket : dgram.Socket;
    localAddress : Object;
    dataChannel : RTCDataChannel;
    constructor(public login : string) {
        this.peerConnection = new RTCPeerConnection();
        this.localSocket = dgram.createSocket('udp4');
        this.localSocket.on('listening', () => {
          this.localAddress = this.localSocket.address();
          console.log("PeerRelay for ${this.login} listening on ${address.address}:${address.port}");
        });
        
        this.dataChannel = this.peerConnection.createDataChannel('faf');
        this.dataChannel.onopen = () => {
            this.dataChannel.log("PeerRelay for ${this.login}: data channel open");
            this.dataChannel.onmessage = (event) => {
              var data = event.data;
              console.log("PeerRelay for ${this.login} received ${event.data}");
              this.localSocket.send(event.data, options.lobby_port, "localhost");   
            }
        }
        
        this.localSocket.bind(0);
        
        this.localSocket.on('message', (msg, rinfo) => {
          console.log(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
          this.dataChannel.send(msg);
        });
   }
}