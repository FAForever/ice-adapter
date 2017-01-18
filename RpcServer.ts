import {Server} from 'json-rpc2';

import options from './options';

class RpcServer {
    server : Server;
    constructor(public firstName, public middleInitial, public lastName) {
        this.server = Server.$create();
        this.server.listenRaw(options.rpc_port, 'localhost');
    }
}