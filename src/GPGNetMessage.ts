
export class GPGNetMessage {

  public header: string;
  public chunks: Array<any>;

  constructor(_header?: string, _chunks?: Array<any>) {
    this.header = _header ? _header : 'uninitialized';
    this.chunks = _chunks ? _chunks : new Array<any>();
  }

  byteCount(): number {
    let result: number = 0;
    result += 4 /* headerSize */
      + this.header.length /* header */
      + 4;  /* chunksize */

    for (let chunk of this.chunks) {
      result += 1; /* typeCode */
      if (typeof chunk === "string") {
        result += (4 + chunk.length);
      }
      else {
        result += 4;
      }
    }
    return result;
  }

  toBuffer(): Buffer {
    let result = Buffer.allocUnsafe(this.byteCount());
    try {
      let offset: number = 0;
      offset = result.writeInt32LE(this.header.length, offset);
      offset += result.write(this.header, offset, this.header.length, 'latin1');
      offset = result.writeInt32LE(this.chunks.length, offset);

      for (let chunk of this.chunks) {
        if (typeof chunk === "string") {
          offset = result.writeInt8(1, offset);
          offset = result.writeInt32LE(chunk.length, offset);
          offset += result.write(chunk, offset, chunk.length, 'latin1');
        }
        else {
          offset = result.writeInt8(0, offset);
          offset = result.writeInt32LE(chunk, offset);
        }
      }
    }
    catch (e) {
      console.error(e);
    }

    return result;
  }

  static fromBuffer(buf: Buffer, callback: (msg: GPGNetMessage) => void): Buffer {
    let result = Buffer.from(buf);

    try {
      while (true) {
        let msg = new GPGNetMessage;
        let offset: number = 0;
        let headerLength = result.readInt32LE(offset);
        offset += 4;
        msg.header = result.toString('latin1', offset, offset + headerLength);
        offset += headerLength;
        let chunkCount = result.readInt32LE(offset);
        offset += 4;
        for (let chunkIndex = 0; chunkIndex < chunkCount; chunkIndex++) {
          let chunkType = result.readInt8(offset);
          offset += 1;
          switch (chunkType) {
            case 0:
              msg.chunks.push(result.readInt32LE(offset));
              offset += 4;
              break;
            case 1:
              let strlen = result.readInt32LE(offset);
              offset += 4;
              const string_chunk = result.toString('latin1', offset, offset + strlen);
              //check length of string, since no RangeError is triggered upon premature end of buffer
              if (string_chunk.length != strlen) {
                return result;
              }
              msg.chunks.push(string_chunk);
              offset += strlen;
              break;

          }
        }
        result = result.slice(offset);
        callback(msg);
      }
    }
    catch (e) {
      if (e instanceof RangeError) {
        /* thrown by Buffer parsing methods like readInt32LE */
      }
      else {
        throw e;
      }
    }

    return result;
  }
}
