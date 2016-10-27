# ❆ faf-ice-adapter ❆

An [ICE](https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment) adapter to proxy Forged Alliance network packets between players.

## JSONRPC Protocol
The `faf-ice-adapter` is controlled using a bi-directional [JSON-RPC](http://www.jsonrpc.org/specification) interface over TCP.
The internal server was tested against [bjsonrpc](https://github.com/deavid/bjsonrpc).

### Methods (client ➠ faf-ice-adapter)

| Name|Parameters|Returns|Description|
|---|---|---|---|
| quit| |"ok"|Gracefully shuts down the `faf-ice-adapter`.|
| hostGame|mapName (string)|"ok"|Tell the game to create the lobby and host game on Lobby-State.|
| joinGame|remotePlayerLogin (string), remotePlayerId (int)|"ok"|Tell the game to create the Lobby, create a PeerRelay and join the remote game.|
| connectToPeer|remotePlayerLogin (string), remotePlayerId (int)|"ok"|Create a PeerRelay and tell the game to connect to the remote peer.|
| setSdp|remotePlayerId (int), sdp64 (string)|"ok"|Set the remote SDP to the PeerRelay to establish a connection.|
| sendToGpgNet|header (string), chunks (array)|"ok"|Send an arbitrary message to the game.|
|status|header (string), chunks (array)|[status structure](#Status structure)|Polls the current status of the `faf-ice-adapter`.|

### Notifications (faf-ice-adapter ➠ client )
| Name|Parameters|Description |
|---|---|---|
| rpcNeedSdp|localPlayerId (int), remotePlayerId (int)|A PeerRelay was created and the SDP record is needed to establish a connection.|
|rpcConnectionStateChanged|"Connected"/"Disconnected" (string)|The game connected to the internal GPGNetServer.|
|rpcGPGNetMessageReceived|header (string), chunks (array)|The game sent a message to the `faf-ice-adapter` via the internal GPGNetServer.|
|rpcGatheredSdp|localPlayerId (int), remotePlayerId (int), SDP (string)|The PeerRelays IceAgent gathered the local SDP record for connecting to the remote player. This Base64 encoded SDP string must be forwarded to the remote peer and set using the `setSdp` command.|
|rpcIceStateChanged|localPlayerId (int), remotePlayerId (int), "NeedRemoteSdp" / "Disconnected" / "Gathering" / "Connecting" / "Connected" / "Ready" / "Failed"|Informs the client about the ICE connectivity state change.|

#### Status structure
```
{
"options" : /* The commandline options */
"gpgnet" : { /* The GPGNet state */
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

## Building `faf-ice-adapter`
`faf-ice-adapter` is using [libnice](https://nice.freedesktop.org/wiki/), a glib-based [ICE](https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment) implementation.
It is intentionally single-threaded to keep things simple. It uses mostly async I/O.

To circumvent mixing of mainloops, the remaining networking part of `faf-ice-adapter` is also using GIO, more precisely glibmm/giomm, to keep C++ development simple.

All current dependencies are
- libnice for ICE
- jsoncpp for JSON
- boost for logging
- giomm for TCP/UDP servers



### Cross compile for Windows
The easy way I used to build the static Windows exe is the remarkable [MXE project](https://github.com/mxe/mxe). These build instructions should work on Linux:
```
git clone https://github.com/muellni/mxe.git mxe
cd mxe && git checkout package_libnice
make MXE_TARGETS=i686-w64-mingw32.static MXE_PLUGIN_DIRS=plugins/gcc6 JOBS=8 -j4 libnice jsoncpp boost glibmm
cd ..
git clone https://github.com/FAForever/ice-adapter.git ice-adapter && cd ice-adapter
mkdir build && cd build
../../mxe/usr/bin/i686-w64-mingw32.static-cmake ..
make -8
```

### From Windows
This should build with MSVC somehow...

## Building for Linux
Install libnice, jsoncpp, boost, giomm (may be part of glibmm) and cmake and compile.
