import options from './options';
import {GPGNetServer} from './GPGNetServer';
import {GPGNetMessage} from './GPGNetMessage';

export class IceAdapter {
    
    gpgNetServer : GPGNetServer;

    constructor() {
        this.gpgNetServer = new GPGNetServer((msg : GPGNetMessage) => {this.onGpgMsg(msg);});
    }

    onGpgMsg(msg : GPGNetMessage) {
        console.log(this);
        switch(msg.header) {
            case 'GameState':
                switch(msg.chunks[0]) {
                    case 'Idle':
                        this.gpgNetServer.send(new GPGNetMessage('CreateLobby', [0,
                                                                                 options.lobby_port,
                                                                                 options.login,
                                                                                 options.id,
                                                                                 1]));
                        break;
                    case 'Lobby':
                        this.gpgNetServer.send(new GPGNetMessage('HostGame', ['Monument Valleys']));
                        break;
                }
                break;
        }
    }
}