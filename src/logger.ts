
import options from './options';
import * as winston from 'winston';

let transports = [new (winston.transports.Console)({
  colorize: 'none',
  level: 'debug',
  timestamp: true,
  debugStdout: true
})];

if (options.logFile.length > 0) {
  transports.push(new (winston.transports.File)({
    filename: options.logFile,
    level: 'debug',
    maxsize: 1e6, /* ~1 MB */
    maxFiles: 3
  }))
};

export let logger = new (winston.Logger)({
  transports: transports
});
export default logger;
