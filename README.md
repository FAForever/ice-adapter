# ❆ faf-ice-adapter ❆

 A P2P connection proxy for Supreme Commander: Forged Alliance using [ICE](https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment).

## JSONRPC Protocol
The `faf-ice-adapter` is controlled using a bi-directional [JSON-RPC](http://www.jsonrpc.org/specification) interface over TCP.

### Methods (client ➠ faf-ice-adapter)

| Name | Parameters | Returns | Description |
| --- | --- | --- | --- |
| quit | | | Gracefully shuts down the `faf-ice-adapter`. |
| hostGame | mapName (string) | | Tell the game to create the lobby and host game on Lobby-State. |
| joinGame | remotePlayerLogin (string), remotePlayerId (int) | | Tell the game to create the Lobby, create a PeerRelay in answer mode and join the remote game. |
| connectToPeer | remotePlayerLogin (string), remotePlayerId (int), offer (bool)| | Create a PeerRelay and tell the game to connect to the remote peer with offer/answer mode. |
| disconnectFromPeer | remotePlayerId (int)| | Create a PeerRelay and tell the game to connect to the remote peer. |
| iceMsg | remotePlayerId (int), msg (object) | | Add the remote ICE message to the PeerRelay to establish a connection. |
| sendToGpgNet | header (string), chunks (array) | | Send an arbitrary message to the game. |
| status | | [status structure](#status-structure) | Polls the current status of the `faf-ice-adapter`. |

### Notifications (faf-ice-adapter ➠ client )
| Name | Parameters | Description |
| --- | --- | --- |
| onConnectionStateChanged | "Connected"/"Disconnected" (string) | The game connected to the internal GPGNetServer. |
| onGpgNetMessageReceived | header (string), chunks (array) | The game sent a message to the `faf-ice-adapter` via the internal GPGNetServer. |
| onIceMsg | localPlayerId (int), remotePlayerId (int), msg (object) | The PeerRelays gathered a local ICE message for connecting to the remote player. This message must be forwarded to the remote peer and set using the `iceMsg` command. |
| onIceConnectionStateChanged | localPlayerId (int), remotePlayerId (int), state (string) | See https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState |
| onDatachannelOpen | localPlayerId (int), remotePlayerId (int) | Informs the client that ICE connectivity to the peer is established. |

#### Status structure
```
{
"version" : /* string: faf-ice-adapter version */
"options" : /* The commandline options */
"gpgnet" : { /* The GPGNet state */
  "local_port" : /* int: The port the game should connect to via /gpgnet 127.0.0.1:port */
  "connected" : /* boolean: Is the game connected? */
  "game_state" : /* string: The last received "GameState" */
  "task_string" : /* string: A string describing the task/role of the game (joining/hosting)*/
  }
"relays" : [/* An array of relay information*/
  {
    "remote_player_id" : /* int: The ID of the remote player */
    "remote_player_login" : /* string: The name of the remote player */
    "local_game_udp_port" : /* int: The UDP port opened for the game to connect to */
    "ice_agent": {/* Information about the IceAgent for this peer */
      "state": /* string: The connection state https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState */
      "datachannel_open": /* bool: Is the data channel open for sending/receiving game packets */
      "loc_cand_addr": /* string: The local address used for the connection */
      "rem_cand_addr": /* string: The remote address used for the connection */
      "loc_cand_type": /* string: The type of the local candidate 'local'/'stun'/'relay' */
      "rem_cand_type": /* string: The type of the remote candidate 'local'/'stun'/'relay' */
      "time_to_connected": /* double: The time it took to connect to the peer in seconds */
      }
    },
  ...
  ]
}
```

## Commandline invocation
The first two commandline arguments `--id` and `--login` must be specified like this: `faf-ice-adapter -i 3 -l "Rhiza"`
The full commandline help text is:
```
faf-ice-adapter usage:
--help                               produce help message
--id arg                             set the ID of the local player
--login arg                          set the login of the local player, e.g. "Rhiza"
--rpc_port arg (=7236)               set the port of internal JSON-RPC server
--gpgnet_port arg (=7237)            set the port of internal GPGNet server
--lobby_port arg (=7238)             set the port the game lobby should use for incoming UDP packets from the PeerRelay
--log_file arg                       set a verbose log file
```

## Example usage sequence

| Step | Player 1 "Alice" | Player 2 "Bob" |
| --- | --- | --- |
| 1 | Start the client |  |
| 2 | The client starts `faf-ice-adapter` and connects to the JSONRPC server |  |
| 3 | The client starts the game and makes it connect to the GPGNet server of the `faf-ice-adapter` using `/gpgnet 127.0.0.1:7237` commandline argument for `ForgedAlliance.exe` |  |
| 4 | The client sends `hostGame('monument_valley.v0001')`||
| 5 | The game should now wait in the lobby and the client receives a lot of `onGpgNetMessageReceived` notifications from `faf-ice-adapter`||
| 6 |  | Now Bob want to join Alices game, starts the client, the client starts `faf-ice-adapter` and the game like Alice did. |
| 7 | The client sends `connectToPeer('Bob', 2, true)` | The client sends `joinGame('Alice', 1)` |
| 8 | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. |
| 9 | The client receives multiple `onIceMsg(1, 2, someIceMsg)`notifications and must now transfer these messages string ordered to the peer. | The client receives multiple `onIceMsg(1, 2, someIceMsg)`notifications and must now transfer these messages string ordered to the peer. |
| 10 | The client must set the transferred ICE messages for the peer using `iceMsg(2, someIceMsg)`. | The client must set the transferred ICE messages for the peer using `iceMsg(1, someIceMsg)`. |
| 11 | The client received multiple `iceConnectionStateChanged(...)` notifications which would finally show the `'Connected'` or `'Complete'` state, which should also let the game connect to the peer. Another indicator for a connection is the `onDatachannelOpen` notification.| The client received multiple `iceConnectionStateChanged(...)` notifications which would finally show the `'Connected'` or `'Complete'` state, which should also let the game connect to the peer. |

## Building `faf-ice-adapter`

## Testing
1. Install nodejs: https://nodejs.org
2. Run `npm install`
4. Run `node src/index.js --id 1 --login 2`
5. Download the TestClient: https://github.com/FAForever/ice-adapter/wiki/Testclient https://github.com/FAForever/ice-adapter/releases
6. Copy the TestClient to the current folder
7. Start the TestClient
8. Press in the Testclient `Connect` at Server
9. Press in the Testclient `Start` at ICE adapter

You can start multiple TestClients.
One client must host the game, the other clients can then join the game.
