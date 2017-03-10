import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel, RTCStatsResponse, RTCOfferOptions } from 'wrtc';
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
  startTime: [number, number];
  connectedTime: [number, number];
  loc_cand_addr: string;
  rem_cand_addr: string;
  loc_cand_type: string;
  rem_cand_type: string;
  lastConnectionAttemptTime: Date;
  connectionAttemptTimeoutMilliseconds: number = 10000;
  receivedOffer: boolean = false;

  constructor(public remoteId: number, public remoteLogin: string, public createOffer: boolean, protected iceServers: Array<any>) {
    super();
    this.startTime = process.hrtime();
    this.iceConnectionState = 'None';
    this.loc_cand_addr = 'none';
    this.rem_cand_addr = 'none';
    this.loc_cand_type = 'none';
    this.rem_cand_type = 'none';
    this.initPeerConnection();
    this.initLocalSocket();

    setInterval(() => { this.checkConnection(); },
      this.connectionAttemptTimeoutMilliseconds);

    logger.info(`PeerRelay for ${remoteLogin}(${remoteId}): created`);
  }

  initPeerConnection() {
    if (this.dataChannel) {
      this.dataChannel.onopen = null;
      this.dataChannel.onclose = null;
      this.dataChannel.close();
      delete this.dataChannel;
    }
    if (this.peerConnection) {
      this.peerConnection.onerror = null;
      this.peerConnection.ondatachannel = null;
      this.peerConnection.onicecandidate = null;
      this.peerConnection.oniceconnectionstatechange = null;
      this.peerConnection.onicegatheringstatechange = null;
      this.peerConnection.onicegatheringstatechange = null;
      this.peerConnection.close();
      delete this.peerConnection;
    }
    this.receivedOffer = false;
    this.peerConnection = new RTCPeerConnection({
      iceServers: this.iceServers
    });
    this.lastConnectionAttemptTime = new Date();
    this.iceConnectionState = this.peerConnection.iceConnectionState;

    this.peerConnection.onerror = (event) => {
      this.handleError("peerConnection.onerror", event);
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
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): iceConnectionState changed to ${this.iceConnectionState}`);
      if ((this.iceConnectionState == 'connected' || this.iceConnectionState == 'completed')
        && !this.connectedTime) {
        this.connectedTime = process.hrtime(this.startTime);
        logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): connection established after ${this.connectedTime[1] / 1e9}s`);

        this.peerConnection.getStats((stats) => {
          stats.result().forEach((stat) => {
            if (stat.type == 'googCandidatePair' &&
              stat.stat('googActiveConnection') === 'true') {
              this.loc_cand_addr = stat.stat('googLocalAddress');
              this.rem_cand_addr = stat.stat('googRemoteAddress');
              this.loc_cand_type = stat.stat('googLocalCandidateType');
              this.rem_cand_type = stat.stat('googRemoteCandidateType');
            }
          });
        },
          (error) => {
            this.handleError("getStats failed", error);
          });

      }

      if (this.iceConnectionState == 'failed') {
        logger.warn(`Relay for ${this.remoteLogin}(${this.remoteId}): Connection failed, forcing reconnect immediately.`);
        setTimeout(() => { this.initPeerConnection(); }, 0);
      }

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
            this.handleError("setLocalDescription with offer failed", error);
          }
        );
      }, (error) => {
        this.handleError("createOffer failed", error);
      });
    }

  }

  initDataChannel(dc: RTCDataChannel) {
    if (this.dataChannel) {
      this.dataChannel.onopen = null;
      this.dataChannel.onclose = null;
      this.dataChannel.close();
      delete this.dataChannel;
    }
    this.dataChannel = dc;
    this.dataChannel.onopen = () => {
      logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): data channel open`);
      this.dataChannel.onmessage = (event) => {
        if (this.localSocket) {
          this.localSocket.send(Buffer.from(event.data), options.lobbyPort, 'localhost', (error, bytes) => {
            if (error) {
              logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}): error sending to local socket: ${JSON.stringify(error)}`);
            }
          });
        }
      }
      this.emit('datachannelOpen');
      if (!this.connectedTime) {
        this.connectedTime = process.hrtime(this.startTime);
        logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): connection established after ${this.connectedTime[1] / 1e9}s`);
      }
    };
    this.dataChannel.onclose = () => {
      logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}): data channel closed`);
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
      //logger.info(`Relay for ${this.remoteLogin}(${this.remoteId}): received msg ${msg} from ${JSON.stringify(rinfo)}}`);
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
    if (msg.type == 'offer') {
      this.receivedOffer = true;
      this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        this.peerConnection.createAnswer().then((desc: RTCSessionDescription) => {
          this.peerConnection.setLocalDescription(new RTCSessionDescription(desc)).then(() => {
            this.emit('iceMessage', desc);
          },
            (error) => {
              this.handleError("setLocalDescription with answer failed", error);
            }
          );
        },
          (error) => {
            this.handleError("createAnswer() for offer failed", error);
          }
        );
      },
        (error) => {
          this.handleError("setRemoteDescription failed", error);
        }
      );
    }
    else if (msg.type == 'answer') {
      this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): set remote answer`);
      },
        (error) => {
          this.handleError("setRemoteDescription failed", error);
        });
    }
    else if (msg.type == 'candidate') {
      this.peerConnection.addIceCandidate(new RTCIceCandidate(msg.candidate)).then(() => {
        logger.debug(`Relay for ${this.remoteLogin}(${this.remoteId}): added ICE candidate ${JSON.stringify(msg.candidate)}`);
      },
        (error) => {
          this.handleError("addIceCandidate failed", error);
        });
    }
    else {
      logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}): unknown ICE message type: ${JSON.stringify(msg)}`);
    }
  }

  handleError(what: string, error) {
    logger.error(`Relay for ${this.remoteLogin}(${this.remoteId}) ${what}: ${JSON.stringify(error)}`);
  }

  close() {
    this.peerConnection.close();
    this.localSocket.close();
  }

  setIceServers(iceServers: Array<Object>) {
    this.iceServers = iceServers;
  }

  checkConnection() {
    if (this.iceConnectionState == 'failed' ||
      this.iceConnectionState == 'new' ||
      this.iceConnectionState == 'checking') {
      if ((new Date().getTime() - this.lastConnectionAttemptTime.getTime()) > this.connectionAttemptTimeoutMilliseconds) {
        if (this.createOffer) {
          logger.warn(`Relay for ${this.remoteLogin}(${this.remoteId}): ice connection state is stuck in offerer. Forcing reconnect...`);
          this.initPeerConnection();
        }
        else if (this.receivedOffer) {
          logger.warn(`Relay for ${this.remoteLogin}(${this.remoteId}): ice connection state is stuck in answerer. Forcing reconnect...`);
          this.initPeerConnection();
        }
      }
    }
  }
}
