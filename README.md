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
| disconnectFromPeer | remotePlayerId (int)| | Destroy PeerRelay and tell the game to disconnect from the remote peer. |
| setLobbyInitMode | lobbyInitMode (string): "normal" or "auto" | | Set the lobby mode the game will use. Supported values are "normal" for normal lobby and "auto" for automatch lobby (aka ladder). |
| iceMsg | remotePlayerId (int), msg (object) | | Add the remote ICE message to the PeerRelay to establish a connection. |
| sendToGpgNet | header (string), chunks (array) | | Send an arbitrary message to the game. |
| setIceServers | iceServers (array) | | ICE server array for use in webrtc. Must be called before joinGame/connectToPeer. See https://developer.mozilla.org/en-US/docs/Web/API/RTCIceServer |
| status | | [status structure](#status-structure) | Polls the current status of the `faf-ice-adapter`. |

### Notifications (faf-ice-adapter ➠ client )
| Name | Parameters | Description |
| --- | --- | --- |
| onConnectionStateChanged | "Connected"/"Disconnected" (string) | The game connected to the internal GPGNetServer. |
| onGpgNetMessageReceived | header (string), chunks (array) | The game sent a message to the `faf-ice-adapter` via the internal GPGNetServer. |
| onIceMsg | localPlayerId (int), remotePlayerId (int), msg (object) | The PeerRelays gathered a local ICE message for connecting to the remote player. This message must be forwarded to the remote peer and set using the `iceMsg` command. |
| onIceConnectionStateChanged | localPlayerId (int), remotePlayerId (int), state (string) | See https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState |
| onConnected | localPlayerId (int), remotePlayerId (int), connected (bool) | Informs the client that ICE connectivity to the peer is established or unestablished. |

#### Status structure
```
{
"version" : /* string: faf-ice-adapter version */
"ice_servers" : /* the ICE servers set using `setIceServers` */
"lobby_port" : /* the actual game lobby UDP port. Should match --lobby-port option if non-zero port is specified. */
"init_mode" : /* the current init mode. See setLobbyInitMode */
"options" : /* The specified commandline options */
"gpgnet" : { /* The GPGNet state */
  "local_port" : /* int: The port the game should connect to via /gpgnet 127.0.0.1:port */
  "connected" : /* boolean: Is the game connected? */
  "game_state" : /* string: The last received "GameState" */
  "task_string" : /* string: A string describing the task/role of the game (joining/hosting)*/
  }
"relays" : [/* An array of relay information for each peer */
  {
    "remote_player_id" : /* int: The ID of the remote player */
    "remote_player_login" : /* string: The name of the remote player */
    "local_game_udp_port" : /* int: The UDP port opened for the game to connect to */
    "ice": {/* ICE state information for this peer */
      "offerer": /* bool: one peer is always offerer, one answerer */
      "state": /* string: The connection state https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState */
      "gathering_state": /* string: The gathering state https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceGatheringState */
      "datachannel_state": /* string: The state of the https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel */
      "connected": /* bool: Is the peer connected? Needs to be in sync with the remote peer. */
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
--rpc-port arg (=7236)               set the port of internal JSON-RPC server
--gpgnet-port arg (=0)               set the port of internal GPGNet server
--lobby-port arg (=0)                set the port the game lobby should use for incoming UDP packets from the PeerRelay
--log-directory arg                  set a log directory to write ice_adapter_0 log files
```

## Example usage sequence

| Step | Player 1 "Alice" | Player 2 "Bob" |
| --- | --- | --- |
| 1 | Start the client |  |
| 2 | The client starts `faf-ice-adapter` and connects to the JSONRPC server |  |
| 3 | The client starts the game and makes it connect to the GPGNet server of the `faf-ice-adapter` using `/gpgnet 127.0.0.1:7237` commandline argument for `ForgedAlliance.exe` |  |
| 3.1 | The game sends `GameState Idle` | |
| 3.2 | The client sends `CreateLobby ...` | |
| 3.3 | The game sends `GameState Lobby` | |
| 4 | The client sends `hostGame('monument_valley.v0001')`||
| 5 | The game should now wait in the lobby and the client receives a lot of `onGpgNetMessageReceived` notifications from `faf-ice-adapter`||
| 6 |  | Now Bob want to join Alices game, starts the client, the client starts `faf-ice-adapter` and the game like Alice did. |
| 6.1 | | The game sends `GameState Idle` |
| 6.2 | | The client sends `CreateLobby ...` |
| 6.3 | | The game sends `GameState Lobby` |
| 7 | The client sends `connectToPeer('Bob', 2, true)` | The client sends `joinGame('Alice', 1)` |
| 8 | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. |
| 9 | The client receives multiple `onIceMsg(1, 2, someIceMsg)`notifications and must now transfer these messages string ordered to the peer. | The client receives multiple `onIceMsg(1, 2, someIceMsg)`notifications and must now transfer these messages string ordered to the peer. |
| 10 | The client must set the transferred ICE messages for the peer using `iceMsg(2, someIceMsg)`. | The client must set the transferred ICE messages for the peer using `iceMsg(1, someIceMsg)`. |
| 11 | The client received multiple `iceConnectionStateChanged(...)` notifications which would finally show the `'Connected'` or `'Complete'` state, which should also let the game connect to the peer. Another indicator for a connection is the `onDatachannelOpen` notification.| The client received multiple `iceConnectionStateChanged(...)` notifications which would finally show the `'Connected'` or `'Complete'` state, which should also let the game connect to the peer. |

## Building `faf-ice-adapter`
###  Linux
Webrtc is build using clang and linked against clangs libc++, so you need to use these for building the ice-adapter.
1. Build [libwebrtc](https://github.com/FAForever/libwebrtc) with `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DWEBRTC_BRANCH_HEAD=refs/branch-heads/62 -DBUILD_TESTS=ON`
2. Build the ice-adapter with `cmake -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DWEBRTC_INCLUDE_DIRS="path/to/webrtc/include" -DWEBRTC_LIBRARIES="path/to/webrtc/lib/libwebrtc.a" -DCMAKE_BUILD_TYPE=Release` and `make`
###  Windows
1. Download and extract [latest libwebrtc win32 release zip file](https://github.com/FAForever/libwebrtc/releases/latest).
2. Install Visual Studio 2015 compilers and open x86 shell.
3. Build the ice-adapter using `cmake -DWEBRTC_INCLUDE_DIRS="path/to/webrtc/include" -DWEBRTC_LIBRARIES="path/to/webrtc/lib/libwebrtc.lib" -DCMAKE_BUILD_TYPE=Release`
