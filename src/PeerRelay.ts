import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, RTCDataChannel, RTCStatsResponse, RTCOfferOptions } from 'wrtc';
import * as dgram from 'dgram';
import options from './options';
import logger from './logger';
import { EventEmitter } from 'events';

export class PeerRelay extends EventEmitter {

  protected peerConnection: RTCPeerConnection;
  protected localSocket: dgram.Socket;
  protected dataChannel: RTCDataChannel;
  protected iceGatheringState: string;
  protected startTime: [number, number];
  protected lastConnectionAttemptTime: Date;
  protected connectionAttemptTimeoutMs: number = 10000;
  protected receivedOffer: boolean = false;
  protected connectionWatchdogTimer: NodeJS.Timer;
  public connectedTime: [number, number];
  public localPort: number;
  public iceConnectionState: string;
  public localCandAddress: string;
  public remoteCandType: string;
  public remoteCandAddress: string;
  public localCandType: string;
  public dataChannelIsOpen: boolean = false;

  constructor(public remoteId: number, public remoteLogin: string, public createOffer: boolean, protected iceServers: Array<any>) {
    super();
    this.startTime = process.hrtime();
    this.iceConnectionState = 'None';
    this.localCandAddress = 'none';
    this.localCandType = 'none';
    this.remoteCandAddress = 'none';
    this.remoteCandType = 'none';
    this.initPeerConnection();
    this.initLocalSocket();

    this.connectionWatchdogTimer = setInterval(() => { this.checkConnection(); }, this.connectionAttemptTimeoutMs);

    this.logMsg(`created`);
  }

  protected logMsg(msg : string, level : string = 'debug') {
    logger.log(level, `Relay for ${this.remoteLogin}(${this.remoteId}): ${msg}`);
  }

  protected initPeerConnection() {
    this.closeDatachannel();
    this.closePeerConnection();
    this.receivedOffer = false;
    this.peerConnection = new RTCPeerConnection({
      iceServers: this.iceServers
    });
    this.lastConnectionAttemptTime = new Date();

    this.peerConnection.onerror = (event) => {
      this.handleError("peerConnection.onerror", event);
    };

    this.peerConnection.ondatachannel = (event) => {
      this.initDataChannel(event.channel);
    };

    this.peerConnection.onicecandidate = (candidate) => {
      if (candidate.candidate) {
        this.logMsg(`announcing candidate ${JSON.stringify(candidate.candidate)}`);
        this.emit('iceMessage', {
          'type': 'candidate',
          'candidate': candidate.candidate
        });
      }
    }

    this.peerConnection.oniceconnectionstatechange = (event) => {
      this.iceConnectionState = this.peerConnection.iceConnectionState;
      this.logMsg(`iceConnectionState changed to ${this.iceConnectionState}`);
      if ((this.iceConnectionState == 'connected' || this.iceConnectionState == 'completed')
        && !this.connectedTime) {
        this.connectedTime = process.hrtime(this.startTime);
        this.logMsg(`connection established after ${this.connectedTime[1] / 1e9}s`);

        this.peerConnection.getStats((stats) => {
          stats.result().forEach((stat) => {
            if (stat.type == 'googCandidatePair' &&
              stat.stat('googActiveConnection') === 'true') {
              this.localCandAddress = stat.stat('googLocalAddress');
              this.remoteCandAddress = stat.stat('googRemoteAddress');
              this.localCandType = stat.stat('googLocalCandidateType');
              this.remoteCandType = stat.stat('googRemoteCandidateType');
            }
          });
        },
          (error) => {
            this.handleError("getStats failed", error);
          });

      }

      if (this.iceConnectionState == 'failed') {
        this.logMsg(`Connection failed, forcing reconnect immediately.`, 'warn');
        setTimeout(() => { this.initPeerConnection(); }, 0);
      }

      this.emit('iceConnectionStateChanged', this.iceConnectionState);
    };

    this.peerConnection.onicegatheringstatechange = (event) => {
      this.iceGatheringState = this.peerConnection.iceGatheringState;
      this.logMsg(`iceGatheringState changed to ${this.iceGatheringState}`);
    };

    if (this.createOffer) {
      this.logMsg(`creating offer`);

      this.initDataChannel(this.peerConnection.createDataChannel('faf', {
        ordered: false,
        maxRetransmits: 0,
      }));

      this.peerConnection.createOffer().then((desc: RTCSessionDescription) => {
        this.peerConnection.setLocalDescription(new RTCSessionDescription(desc)).then(() => {
          this.emit('iceMessage', desc);
          this.logMsg(`sending offer ${desc}`);
        },
          (error) => {
            this.handleError("setLocalDescription with offer failed", error);
          }
        );
      }, (error) => {
        this.handleError("createOffer failed", error);
      });
    }

    this.iceConnectionState = this.peerConnection.iceConnectionState;
    this.emit('iceConnectionStateChanged', this.iceConnectionState);

  }

  protected initDataChannel(dc: RTCDataChannel) {
    this.closeDatachannel();
    this.dataChannel = dc;
    this.dataChannel.onopen = () => {
      this.dataChannelIsOpen = true;
      this.logMsg(`data channel open`);
      this.dataChannel.onmessage = (event) => {
        if (this.localSocket) {
          this.localSocket.send(Buffer.from(event.data), options.lobbyPort, 'localhost', (error, bytes) => {
            if (error) {
              this.logMsg(`error sending to local socket: ${JSON.stringify(error)}`, 'error');
            }
          });
        }
      }
      this.emit('datachannelOpen');
      if (!this.connectedTime) {
        this.connectedTime = process.hrtime(this.startTime);
        this.logMsg(`connection established after ${this.connectedTime[1] / 1e9}s`);
      }
    };
    this.dataChannel.onclose = () => {
      this.dataChannelIsOpen = false;
      this.logMsg(`data channel closed`);
      delete this.dataChannel;
    }
  }

  protected initLocalSocket() {
    this.localSocket = dgram.createSocket('udp4');

    this.localSocket.bind(undefined, 'localhost', () => {
      this.localPort = this.localSocket.address().port;
      this.logMsg(`listening on port ${this.localPort}`);
      this.emit('localSocketListening');
    });

    this.localSocket.on('message', (msg, rinfo) => {
      if (this.dataChannel) {
        this.dataChannel.send(msg);
      }
    });

    this.localSocket.on('error', (error) => {
      this.logMsg(`error in localsocket: ${JSON.stringify(error)}`);
    });
    this.localSocket.on('close', () => {
      this.logMsg(`local socket closed`);
      delete this.localSocket;
    });
  }


  protected handleError(what: string, error) {
    this.logMsg(`${what}: ${JSON.stringify(error)}`, 'error');
  }

  protected checkConnection() {
    if (this.iceConnectionState == 'failed' ||
        this.iceConnectionState == 'new') {

      let timeSinceLastConnectionAttemptMs = new Date().getTime() - this.lastConnectionAttemptTime.getTime();
      if (timeSinceLastConnectionAttemptMs > this.connectionAttemptTimeoutMs) {
        if (this.createOffer) {
          this.logMsg(`ICE connection state is stuck in offerer. Forcing reconnect...`, 'warn');
          this.initPeerConnection();
        }
        else if (this.receivedOffer) {
          this.logMsg(`ICE connection state is stuck in answerer. Forcing reconnect...`, 'warn');
          this.initPeerConnection();
        }
      }
    }
  }

  protected closeDatachannel() {
    if (this.dataChannel) {
      this.dataChannel.onopen = null;
      this.dataChannel.onclose = null;
      this.dataChannel.close();
      delete this.dataChannel;
    }
    this.dataChannelIsOpen = false;
  }

  protected closePeerConnection() {
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
  }

  public addIceMsg(msg: any) {
    this.logMsg(`received ICE msg: ${JSON.stringify(msg)}`);
    if (msg.type == 'offer') {
      this.receivedOffer = true;
      this.logMsg(`received remote offer`);
      this.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        this.logMsg(`creating answer`);
        this.peerConnection.createAnswer().then((desc: RTCSessionDescription) => {
          this.peerConnection.setLocalDescription(new RTCSessionDescription(desc)).then(() => {
            this.emit('iceMessage', desc);
            this.logMsg(`sending answer ${desc}`);
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
        this.logMsg(`set remote answer`);
      },
        (error) => {
          this.handleError("setRemoteDescription failed", error);
        });
    }
    else if (msg.type == 'candidate') {
      this.peerConnection.addIceCandidate(new RTCIceCandidate(msg.candidate)).then(() => {
        this.logMsg(`adding remote candidate ${JSON.stringify(msg.candidate)}`);
      },
        (error) => {
          this.handleError("addIceCandidate failed", error);
        });
    }
    else {
      this.logMsg(`unknown ICE message type: ${JSON.stringify(msg)}`, 'error');
    }
  }

  public close() {
    clearInterval(this.connectionWatchdogTimer);
    this.localSocket.close();
    this.closeDatachannel();
    this.closePeerConnection();
  }

  public setIceServers(iceServers: Array<Object>) {
    this.iceServers = iceServers;
  }
}
