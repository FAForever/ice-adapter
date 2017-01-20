
import options from './options';
import * as winston from 'winston';

let transports = [new (winston.transports.Console)({
  colorize: 'none',
  level: 'debug',
  timestamp: true,
  debugStdout: true
})];

if (options.log_file.length > 0) {
  transports.push(new (winston.transports.File)({
    filename: options.log_file
  }))
};

export let logger = new (winston.Logger)({
  transports: transports
});
export default logger;
