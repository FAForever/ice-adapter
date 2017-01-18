#!/usr/bin/env node

import options from './options';
import * as winston from 'winston';

if (options.log_file.length > 0) {
  winston.add(winston.transports.File, { filename: options.log_file });
}

winston.info(`Starting faf-ice-adapter version ${require('./package.json').version}`);

import {IceAdapter} from './IceAdapter';

let a = new IceAdapter();

