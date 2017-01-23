import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel, RTCStatsResponse } from 'wrtc';
import * as dgram from 'dgram';
import options from './options';
import logger from './logger';
import { EventEmitter } from 'events';

export class PeerRelay extends EventEmitter {
  peerConnection: RTCPeerConnection;
  localSocket: dgram.Socket;
  localPort: number;
  dataChannel: RTCDataChannel;
  iceConnectionState: string;
  iceGatheringState: string;
  iceMsgHistory: string;
  startTime: [number, number];
  connectedTime: [number, number];
  loc_cand_addr: string;
  rem_cand_addr: string;
  constructor(public remoteId: number, public remoteLogin: string, public createOffer: boolean) {
    super();
    this.iceMsgHistory = '';
    this.startTime = process.hrtime();
    this.iceConnectionState = 'None';
    this.loc_cand_addr = 'none';
    this.rem_cand_addr = 'none';

    this.initPeerConnection();
    this.initLocalSocket();

    logger.info(`Relay for ${remoteLogin}(${remoteId}): successfully created`);
  }

  initPeerConnection() {
    /* https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/ 
       TURN servers require credentials with webrtc! */
    let iceServers = [
      {
        urls: [`turn:${options.turn_server}?transport=tcp`],
        credential: options.turn_pass,
        username: options.turn_user
      },
      {
        urls: [`turn:${options.turn_server}?transport=udp`],
        credential: options.turn_pass,
        username: options.turn_user
      },
      {
        urls: [`stun:${options.stun_server}`],
        credential: '',
        username: ''
      },
  /*
      {
        urls: [`turn:numb.viagenie.ca?transport=tcp`],
        credential: 'asdf',
        username: 'mm+viagenie.ca@netlair.de'
      },
      {
        urls: [`turn:numb.viagenie.ca?transport=udp`],
        credential: 'asdf',
        username: 'mm+viagenie.ca@netlair.de'
      },
      {
        urls: [`stun:numb.viagenie.ca`],
        credential: '',
        username: ''
      }*/];

    logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): iceServers: ${JSON.stringify(iceServers)}`);
    this.peerConnection = new RTCPeerConnection({
      iceServers: iceServers
    });

    this.peerConnection.onerror = (event) => {
      this.handleError(event);
    };

    this.peerConnection.ondatachannel = (event) => {
      this.initDataChannel(event.channel);
    };

    this.peerConnection.onicecandidate = (candidate) => {
      if (candidate.candidate) {
        logger.debug(`PeerRelay for ${this.remoteLogin} received candidate ${JSON.stringify(candidate.candidate)}`);
        this.emit('iceMessage', {
          'type': 'candidate',
          'candidate': candidate.candidate
        });
      }
    }

    this.peerConnection.oniceconnectionstatechange = (event) => {
      this.iceConnectionState = this.peerConnection.iceConnectionState;
      logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): iceConnectionState changed to ${this.iceConnectionState}`);
      if (this.iceConnectionState == 'connected' && !this.connectedTime) {
        this.connectedTime = process.hrtime(this.startTime);
        logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): connection established after ${this.connectedTime[1] / 1e9}s`);
      }

      let reqFields = ['googLocalAddress',
        'googLocalCandidateType',
        'googRemoteAddress',
        'googRemoteCandidateType'
      ];
      this.peerConnection.getStats((stats) => {
        stats.result().forEach((stat) => {
          if (stat.type == 'googCandidatePair' &&
              stat.stat('googActiveConnection') === 'true') {
                this.loc_cand_addr = stat.stat('googLocalAddress');
                this.rem_cand_addr = stat.stat('googRemoteAddress');
                /*
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): active:  ${stat.stat('googActiveConnection')}`);
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): locadd:  ${stat.stat('googLocalAddress')}`);
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): remadd:  ${stat.stat('googRemoteAddress')}`);
              */
          }
        /*
        this.stats = stats.result();
        */
                /*
        let r = stats.result();
        logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): r: ${JSON.stringify(r)}, ${JSON.stringify(Object.getOwnPropertyNames(r))}`);
        r.forEach((stat) => {
          if (stat.type == 'googCandidatePair') {
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): active:  ${stat.stat('googActiveConnection')}`);
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): locadd:  ${stat.stat('googLocalAddress')}`);
              logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): remadd:  ${stat.stat('googRemoteAddress')}`);
          }
              */
          /*
          if (stat.type == 'remotecandidate') {
            logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): remotecandidate: ${JSON.stringify(stat.getValue())}`);
          }
          */
        });

        //console.log(r.stat('googLocalAddress'));
        //console.log(r.stat('googLocalCandidateType'));
        //console.log(r.stat('googRemoteAddress'));
        //console.log(r.stat('googRemoteCandidateType'));
        /*
        let filtered = stats.result().filter(function(e) { return e.id.indexOf('Conn-audio') == 0 && e.stat('googActiveConnection') == 'true' })[0];
        let res = {};
        reqFields.forEach(function(e) { res[e.replace('goog', '')] = filtered.stat(e) });
        logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): stats: ${JSON.stringify(res)}`);
        */
      },
        (error) => {
          this.handleError(error);
        });

      this.emit('iceConnectionStateChanged', this.iceConnectionState);
    };

    this.peerConnection.onicegatheringstatechange = (event) => {
      this.iceGatheringState = this.peerConnection.iceGatheringState;
      logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): iceGatheringState changed to ${this.iceGatheringState}`);
    };

    if (this.createOffer) {
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): create offer`);

      this.initDataChannel(this.peerConnection.createDataChannel('faf', {
        ordered: false,
        maxRetransmits: 0,
      }));

      this.peerConnection.createOffer().then((desc: RTCSessionDescription) => {
        this.peerConnection.setLocalDescription(new RTCSessionDescription(desc)).then(() => {
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
  }

  initDataChannel(dc: RTCDataChannel) {
    dc.onopen = () => {
      this.dataChannel = dc;
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): data channel open`);
      this.dataChannel.onmessage = (event) => {
        if (this.localSocket) {
          this.localSocket.send(Buffer.from(event.data), options.lobby_port, 'localhost', (error, bytes) => {
            if (error) {
              logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}): error sending to local socket: ${JSON.stringify(error)}`);
            }
          });
        }
      }
      this.emit('datachannelOpen');
    };
    dc.onclose = () => {
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): data channel close`);
      if (this.dataChannel) {
        delete this.dataChannel;
      }
    }
  }

  initLocalSocket() {
    this.localSocket = dgram.createSocket('udp4');

    this.localSocket.bind(undefined, 'localhost', () => {
      this.localPort = this.localSocket.address().port;
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): listening on port ${this.localPort}`);
      this.emit('localSocketListening');
    });

    this.localSocket.on('message', (msg, rinfo) => {
      if (this.dataChannel) {
        this.dataChannel.send(msg);
      }
    });

    this.localSocket.on('error', (error) => {
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): error in localsocket: ${JSON.stringify(error)}`);
    });
    this.localSocket.on('close', () => {
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): local socket closed`);
      delete this.localSocket;
    });
  }

  addIceMsg(msg: any) {
    logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): received ICE msg: ${JSON.stringify(msg)}`);
    this.iceMsgHistory += JSON.stringify(msg) + '\n';
    if (msg.type == 'offer') {
      this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        this.peerConnection.createAnswer().then((desc: RTCSessionDescription) => {
          this.peerConnection.setLocalDescription(new RTCSessionDescription(desc)).then(() => {
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
      this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): set remote answer`);
      },
        (error) => {
          this.handleError(error);
        });
    }
    else if (msg.type == 'candidate') {
      this.peerConnection.addIceCandidate(new RTCIceCandidate(msg.candidate)).then(() => {
        logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): added ICE candidate ${JSON.stringify(msg.candidate)}`);
      },
        (error) => {
          this.handleError(error);
        });
    }
    else {
      logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}): unknown ICE message type: ${JSON.stringify(msg)}`);
    }
  }

  handleError(error) {
    logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}) error: ${JSON.stringify(error)}`);
  }

  close() {
    this.peerConnection.close();
    this.localSocket.close();
  }
}
