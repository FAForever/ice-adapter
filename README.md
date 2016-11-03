# ❆ faf-ice-adapter ❆

An [ICE](https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment) adapter to proxy Forged Alliance network packets between players.

## JSONRPC Protocol
The `faf-ice-adapter` is controlled using a bi-directional [JSON-RPC](http://www.jsonrpc.org/specification) interface over TCP.
The internal server was tested against [bjsonrpc](https://github.com/deavid/bjsonrpc).

### Methods (client ➠ faf-ice-adapter)

| Name | Parameters | Returns | Description |
| --- | --- | --- | --- |
| quit | | "ok" | Gracefully shuts down the `faf-ice-adapter`. |
| hostGame | mapName (string) | "ok" | Tell the game to create the lobby and host game on Lobby-State. |
| joinGame | remotePlayerLogin (string), remotePlayerId (int) | "ok" | Tell the game to create the Lobby, create a PeerRelay and join the remote game. |
| connectToPeer | remotePlayerLogin (string), remotePlayerId (int)| "ok" | Create a PeerRelay and tell the game to connect to the remote peer. |
| disconnectFromPeer | remotePlayerId (int)| "ok" | Create a PeerRelay and tell the game to connect to the remote peer. |
| setSdp | remotePlayerId (int), sdp64 (string) | "ok" | Set the remote SDP to the PeerRelay to establish a connection. |
| sendToGpgNet | header (string), chunks (array) | "ok" | Send an arbitrary message to the game. |
| status | header (string), chunks (array) | [status structure](#Status structure) | Polls the current status of the `faf-ice-adapter`. |

### Notifications (faf-ice-adapter ➠ client )
| Name | Parameters | Description |
| --- | --- | --- |
| onNeedSdp | localPlayerId (int), remotePlayerId (int) | A PeerRelay was created and the SDP record is needed to establish a connection. |
| onConnectionStateChanged | "Connected"/"Disconnected" (string) | The game connected to the internal GPGNetServer. |
| onGpgNetMessageReceived | header (string), chunks (array) | The game sent a message to the `faf-ice-adapter` via the internal GPGNetServer. |
| onSdpGathered | localPlayerId (int), remotePlayerId (int), SDP (string) | The PeerRelays IceAgent gathered the local SDP record for connecting to the remote player. This Base64 encoded SDP string must be forwarded to the remote peer and set using the `setSdp` command. |
| onPeerStateChanged | localPlayerId (int), remotePlayerId (int), "NeedRemoteSdp" / "Disconnected" / "Gathering" / "Connecting" / "Connected" / "Ready" / "Failed" | Informs the client about the ICE connectivity state change. |

#### Status structure
```
{
"options" : /* The commandline options */
"gpgnet" : { /* The GPGNet state */
  "local_port" : /*int: The port the game should connect to via /gpgnet 127.0.0.1:port*/
  "connected" : /*boolean: Is the game connected?*/
  "game_state" : /*string: The last received "GameState"*/
  "host_game" : {  /*optional, only in hosting mode*/
    "map" : /*string: The scenario to host*/
    }
  "join_game" : {  /*optional, only in joining mode*/
    "remote_player_login" : /*string: The name of the player to connect to*/
    "remote_player_id" : /*int: The ID of the remote player*/
    }
  }
"relays" : [/* An array of relay information*/
  {
    "remote_player_id" : /*int: The ID of the remote player*/
    "local_game_udp_port" : /*int: The UDP port opened for the game to connect to*/
    "ice_agent": {/*Information about the IceAgent for this peer */
      "state": /*string: The connection state*/
      "connected": /*bool: The connection state is "Ready" and bidirectional communication between the peera is established*/
      "local_candidate": /*string: The local connection information negotiated */
      "remote_candidate": /*string: The remote connection information negotiated */
      "local_sdp": /*string: The unencoded local SDP*/
      "local_sdp64": /*string: The Base64 encoded local SDP*/
      "remote_sdp64": /*string: The Base64 encoded remote SDP*/
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
  --help                                produce help message
  -i [ --id ] arg                       set the ID of the local player
  -l [ --login ] arg                    set the login of the local player, e.g. "Rhiza"
  -p [ --rpc-port ] arg (=7236)         set the port of internal JSON-RPC server
  -g [ --gpgnet-port ] arg (=7237)      set the port of internal GPGNet server
  -o [ --lobby-port ] arg (=7238)       set the port the game lobby should use for incoming UDP packets from the PeerRelay
  -s [ --stun-host ] arg (=dev.faforever.com) set the STUN hostname
  -t [ --turn-host ] arg (=dev.faforever.com) set the TURN hostname
  -u [ --turn-user ] arg                set the TURN username
  -x [ --turn-pass ] arg                set the TURN password
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
| 7 | The client sends `connectToPeer('Bob', 2)` | The client sends `joinGame('Alice', 1)` |
| 8 | The client receives `onNeedSdp(1,2)` | The client receives `rpcNeedSdp(2,1)` |
| 9 | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. | The game should connect to the internal PeerRelay, and shows that it's connecting to the peer. |
| 10 | After some time the client receives `onSdpGathered(1,2,'asdf')` and must now transfer the SDP string to the peer. | After some time the client receives `onSdpGathered(2,1,'qwer')` and must now transfer the SDP string to the peer. |
| 11 | The client must set the transferred SDP for the peer using `setSdp(2, 'qwer')`. | The client must set the transferred SDP for the peer using `setSdp(1, 'asdf')`. |
| 12 | The client received multiple `onPeerStateChanged(...)` notifications which would finally show the `'Ready'` state, which should also let the game connect to the peer. | The client received multiple `onPeerStateChanged(...)` notifications which would finally show the `'Ready'` state, which should also let the game connect to the peer. |

## Building `faf-ice-adapter`
`faf-ice-adapter` is using [libnice](https://nice.freedesktop.org/wiki/), a glib-based [ICE](https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment) implementation.
`faf-ice-adapter` is intentionally single-threaded to keep things simple.
`faf-ice-adapter` uses mostly async I/O to keep things responsive.

To circumvent mixing of mainloops, the remaining networking part of `faf-ice-adapter` is also using gio, more precisely [glibmm/giomm](https://developer.gnome.org/glibmm/stable/), to keep C++ development simple.

All current dependencies are
- libnice for ICE
- jsoncpp for JSON
- boost for logging (totally negotiable, since boost is not a leightwight dependency)
- giomm for TCP/UDP servers

### Cross compile for Windows
The easy way I used to build the static Windows exe is the remarkable [MXE project](https://github.com/mxe/mxe). These build instructions should work on Linux:
```
git clone https://github.com/mxe/mxe.git mxe
cd mxe
make MXE_TARGETS=i686-w64-mingw32.static MXE_PLUGIN_DIRS=plugins/gcc6 JOBS=8 -j4 libnice jsoncpp boost glibmm
cd ..
git clone https://github.com/FAForever/ice-adapter.git ice-adapter && cd ice-adapter
mkdir build && cd build
../../mxe/usr/bin/i686-w64-mingw32.static-cmake ..
make -8
```

### From Windows
The easiest way seems to be MSYS2, but it should also compile with MSVC.

## Building for Linux
Install libnice, jsoncpp, boost, giomm (may be part of glibmm) and cmake and compile.
