import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel } from 'wrtc';
import * as dgram from 'dgram';
import options from './options';
import * as winston from 'winston';
import { EventEmitter } from 'events';

export class PeerRelay extends EventEmitter {
  peerConnection: RTCPeerConnection;
  localSocket: dgram.Socket;
  localPort: number;
  dataChannel: RTCDataChannel;
  constructor(public remoteId: number, public remoteLogin: string, public createOffer: boolean) {
    super();
    let iceServer = { urls: [`stun:${options.stun_server}`, `turn:${options.stun_server}`] };
    if (options.turn_user != '') {
      iceServer['username'] = options.turn_user;
      iceServer['credential'] = options.turn_pass;
    }

    this.peerConnection = new RTCPeerConnection({
      iceServers: [iceServer]
    });

    this.dataChannel = this.peerConnection.createDataChannel('faf');
    this.dataChannel.onopen = () => {
      winston.info(`Relay for ${this.remoteLogin}(${this.remoteId}): data channel open`);
      this.dataChannel.onmessage = (event) => {
        var data = event.data;
        winston.debug("PeerRelay for ${this.login} received ${event.data}");
        this.localSocket.send(event.data, options.lobby_port, "localhost");
      }
    }

    this.peerConnection.onicecandidate = (candidate) => {
      if (candidate.candidate) {
        winston.error(`PeerRelay for ${remoteLogin} received candidate ${JSON.stringify(candidate.candidate)}`);
      }
    }

    if (createOffer) {
      winston.info(`Relay for ${remoteLogin}(${remoteId}): create offer`);
      this.peerConnection.createOffer((desc: RTCSessionDescription) => {
        this.peerConnection.setLocalDescription(
          new RTCSessionDescription(desc),
          () => {
            this.emit('iceMessage', desc);
          },
          (error) => {
            this.handleError(error);
          }
        );
      }, (error) => {
        this.handleError(error);
      });
    }

    this.localSocket = dgram.createSocket('udp4');
    this.localSocket.bind(undefined, 'localhost', () => {
      this.localPort = this.localSocket.address().port;
      winston.debug(`Relay for ${remoteLogin}(${remoteId}): listening on port ${this.localPort}`);
    });

    this.localSocket.on('message', (msg, rinfo) => {
      winston.debug(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
      this.dataChannel.send(msg);
    });
    winston.info(`Relay for ${remoteLogin}(${remoteId}): successfully created`);
  }

  addIceMsg(msg: any) {
    winston.info(`Relay for ${this.remoteLogin}(${this.remoteId}): received ICE msg: ${JSON.stringify(msg)}`);

    if (msg.type == 'offer') {
      this.peerConnection.setRemoteDescription(
        new RTCSessionDescription(msg),
        () => {
          this.peerConnection.createAnswer((desc: RTCSessionDescription) => {
            this.peerConnection.setLocalDescription(
              new RTCSessionDescription(desc),
              () => {
                this.emit('iceMessage', desc);
              },
              (error) => {
                this.handleError(error);
              }
            );
          },
            (error) => {
              this.handleError(error);
            }
          );
        },
        (error) => {
          this.handleError(error);
        }
      );
    }
    else if (msg.type == 'answer') {
      this.peerConnection.setRemoteDescription(
        new RTCSessionDescription(msg),
        () => {
          winston.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): set remote answer`);
        },
        (error) => {
          this.handleError(error);
        });
    }
  }

  handleError(error) {
    winston.error(`Relay for ${this.remoteLogin}(${this.remoteId}) error: ${JSON.stringify(error)}`);
  }
}
