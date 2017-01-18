import {createServer, Socket, Server} from 'net';
import options from './options';
import {GPGNetMessage} from './GPGNetMessage';


export class GPGNetServer {
    
    socket : Socket;
    
    buffer : Buffer;
    
    constructor(callback: (msg : GPGNetMessage) => void ) {
        
        this.buffer = Buffer.alloc(0);
        
        createServer( (socket) => {
            console.log('GPGNet client connected');
            this.socket = socket;
            this.socket.on('data', (data : Buffer) => {
                console.log(`GPGNet server received ${data.toString('hex')}`);
                this.buffer = Buffer.concat([this.buffer, data], this.buffer.length + data.length);
                this.buffer = GPGNetMessage.fromBuffer(this.buffer, callback);
            });
            
            this.socket.on('close', (had_error) => {
                console.log('GPGNet client disconnected');
            });
        }).listen(options.gpgnet_port, 'localhost', () => {
            console.log(`GPGNet server listening on port ${options.gpgnet_port}`);
        });
    }
    
    send(msg : GPGNetMessage) {
        let bufToSend : Buffer = msg.toBuffer();
        console.log(`GPGNet server sending ${bufToSend.toString('hex')}`);
        GPGNetMessage.fromBuffer(bufToSend, (msg : GPGNetMessage) => {
            console.log(`GPGNet server sending ${msg}`);
        });
        this.socket.write(bufToSend);
    }
    
}