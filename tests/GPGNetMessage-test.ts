import {GPGNetMessage} from '../GPGNetMessage';
import {expect} from 'chai';


let msg1 = new GPGNetMessage('CreateLobby', [0, 1234, 'Rhyza', 5432, 1]);
let msg2 = new GPGNetMessage('HostGame', ['map12345.map']);

let buf1 = msg1.toBuffer();
let buf2 = msg2.toBuffer();

let parsedMsgs = new Array<GPGNetMessage>();

let buf1_parsed = GPGNetMessage.fromBuffer(buf1, (msg : GPGNetMessage) => {
    expect(msg1).to.eql(msg);
    parsedMsgs.push(msg);
});
expect(buf1_parsed).to.have.lengthOf(0);

let buf2_parsed = GPGNetMessage.fromBuffer(buf2, (msg : GPGNetMessage) => {
    expect(msg2).to.eql(msg);
    parsedMsgs.push(msg);
});
expect(buf2_parsed).to.have.lengthOf(0);

expect(parsedMsgs).to.have.lengthOf(2);

parsedMsgs = [];
let concatBuf = Buffer.concat([buf1, buf2], buf1.length + buf2.length);
let concatBuf_parsed = GPGNetMessage.fromBuffer(concatBuf, (msg : GPGNetMessage) => {
    parsedMsgs.push(msg);
});
expect(msg1).to.eql(parsedMsgs[0]);
expect(msg2).to.eql(parsedMsgs[1]);

