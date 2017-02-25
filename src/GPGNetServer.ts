import { createServer, Socket, Server } from 'net';
import options from './options';
import { GPGNetMessage } from './GPGNetMessage';
import logger from './logger';
import { EventEmitter } from 'events';

export class GPGNetServer extends EventEmitter {

  public server: Server;
  public socket: Socket;

  buffer: Buffer;

  constructor(callback: (msg: GPGNetMessage) => void) {
    super();

    this.buffer = Buffer.alloc(0);

    this.server = createServer((socket) => {
      logger.info('GPGNet client connected');
      this.emit('connected');
      this.socket = socket;
      this.socket.on('data', (data: Buffer) => {
        this.buffer = Buffer.concat([this.buffer, data]);
        this.buffer = GPGNetMessage.fromBuffer(this.buffer, callback);
      });

      this.socket.on('close', (had_error) => {
        logger.info('GPGNet client disconnected');
        this.emit('disconnected');
        if (this.socket) {
          delete this.socket;
        }
      });

      this.socket.on('error', (error) => {
        logger.error(`GPGNet client socket error: ${JSON.stringify(error)}`);
      });
    }).listen(options.gpgnetPort, 'localhost', () => {
      logger.info(`GPGNet server listening on port ${this.server.address().port}`);
    });
  }

  send(msg: GPGNetMessage) {
    logger.info(`GPGNet server sending msg ${JSON.stringify(msg)}`);
    if (this.socket) {
      this.socket.write(msg.toBuffer());
    }
  }

}
