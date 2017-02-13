import {GPGNetMessage} from '../src/GPGNetMessage';
import {expect} from 'chai';


const msg1 = new GPGNetMessage('CreateLobby', [0, 1234, 'Rhyza', 5432]);
const msg2 = new GPGNetMessage('HostGame', ['map12345.map']);

const buf1 = msg1.toBuffer();
const buf2 = msg2.toBuffer();

let parsedMsgs = new Array<GPGNetMessage>();

/* parse msg1 */
let buf1_parsed = GPGNetMessage.fromBuffer(buf1, (msg : GPGNetMessage) => {
    expect(msg1).to.eql(msg);
    parsedMsgs.push(msg);
});
expect(buf1_parsed).to.have.lengthOf(0);

/* parse msg2 */
let buf2_parsed = GPGNetMessage.fromBuffer(buf2, (msg : GPGNetMessage) => {
    expect(msg2).to.eql(msg);
    parsedMsgs.push(msg);
});
expect(buf2_parsed).to.have.lengthOf(0);

expect(parsedMsgs).to.have.lengthOf(2);

/* parse both messages from concatenated buffer */
parsedMsgs = [];
let concatBuf = Buffer.concat([buf1, buf2]);
let concatBuf_parsed = GPGNetMessage.fromBuffer(concatBuf, (msg : GPGNetMessage) => {
    parsedMsgs.push(msg);
});
expect(msg1).to.eql(parsedMsgs[0]);
expect(msg2).to.eql(parsedMsgs[1]);


/* split first buffer into half and test partial parsing */
const msg3 = new GPGNetMessage('JsonStats', ['really longs string asdfsfd sdfjklsfdklssfdjksfdaj;kj;sfdajdjksfdajksfdajsfdajsfdasfdasfdaasfdjlsfdajlfaiwowfaijwfeaiowajfeowfajiwfeaiociowamiwoasdkokcakcaokscaox;lksdksa;oxldiskifdjfdusjsfdasfdaukh']);
const buf3 = msg3.toBuffer();

const buf3_part1 = buf3.slice(0, buf3.length/2);
const buf3_part2 = buf3.slice(buf3.length/2, buf3.length);
const buf3_part_parsed1 = GPGNetMessage.fromBuffer(buf3_part1, (msg : GPGNetMessage) => {
    expect(false).to.equal(true);
});
/* the returned buffer after parsing should be untouched */
expect(buf3_part1.length).to.equal(buf3_part_parsed1.length);

/* now concat both parts and expect to parse message1 */
const buf3_part_concat = Buffer.concat([buf3_part1, buf3_part2]);
expect(buf3_part_concat.length).to.equal(buf3.length);
parsedMsgs = [];
const buf3_part_parsed2 = GPGNetMessage.fromBuffer(buf3_part_concat, (msg : GPGNetMessage) => {
    expect(msg).to.eql(msg3);
    parsedMsgs.push(msg);
});
expect(msg3).to.eql(parsedMsgs[0]);
expect(buf3_part_parsed2).to.have.lengthOf(0);
