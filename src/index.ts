import options from './options';
import logger from './logger';

logger.info(`Starting faf-ice-adapter version ${require('../package.json').version}`);

import { IceAdapter } from './IceAdapter';

let a = new IceAdapter();

