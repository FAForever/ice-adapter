const UglifyJSPlugin = require('uglifyjs-webpack-plugin');
const webpack = require('webpack');

module.exports = {  
  entry: './src/index.js',
  output: {
    path: __dirname + "/dist",
    filename: 'faf-ice-adapter.js'
  },
  module: {
    loaders: [
      { test: /\.js$/, loader: 'shebang-loader' },
      { test: /\.json$/, loader: 'json-loader' }
    ]
  },
  target: 'node',
  externals: [
    { 'require-main-filename': true },
    { './wrtc.node': './wrtc.node' }
  ],
  plugins: [
    new UglifyJSPlugin(),
    new webpack.BannerPlugin('#!/usr/bin/env node', { raw: true })
  ]
}
