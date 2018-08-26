v6.3.2
======

- update to webrtc 68
- connectivitychecker ping now has some startup time

v6.3.1
======

- fix messed up TURN connections

v6.3.0
======

- reconnect is now back to recreating the whole PeerConnection instead
  of using ICE restart to fix memory leak issues

v6.2.6
======

- fix stats not always collected
- refactor connectivitychecker
- print version on commandline

v6.2.5
======

- enable travis CI build for linux binary

v6.2.4
======

- build against current libwebrtc

v6.2.3
======

- adjust for webrtc 66
- improve GPGNetMessage parsing

v6.2.2
======

- use a custom ping

v6.2.1
======

- more work on reconnect
- more status information

v6.2.0
======

- change reconnect behaviour

v6.1.3
======

- fixing the GPGNetServer
- a lot more testing

v6.1.2
======

- fix not using ICE servers

v6.1.1
======

- testing and debugging

v6.1.0
======

- fix accidentially running multiple threads
- fix automatic lobby port
- create new faf-ice-testclient executable to allow unattended testing

v6.0.0
======

- rewrite as pure C++ implementation only depending on a native WebRTC build

v5.6.1
======

- use updates webrtc

v5.6.0
======

- remove Testclient and simplify CI build

v5.5.3
======

- refactor logging in PeerRelay

v5.5.2
======

- #44 don't interrupt 'checking' state after 10 second timeout
- #43 fix logfile size

v5.5.1
======

- do more logging

v5.5.0
======

- add `setInitMode` API method to enable auto-lobby for ladder/matchmaking

v5.4.2
======

- use updated node-webrtc binaries

v5.4.1
======

- rework closing RTCPeerConnections to fix #34

v5.4.0
======

- create `setIceServers` API method, which must be called before connecting to peers
- work on reconnect

v5.3.1
======

- create better platform archives

v5.3.0
======

- initial release of automatic reconnect in testclient, testserver and ice-adapter
- create this CHANGELOG

v5.2.0
======

- use hyphens instead of underscore in options
- fix build of `faf-ice-testclient` on travis CI

v5.1.4
======

- make the established peer connection type accessible
- add creation of dummy peerrelay at ice-adapter startup to trigger windows firewall prompt

v5.1.3
======

- fix long string parsing buffer error in GPGNetMessage which made subsequent games fail
- reset gpgnetstate

v5.1.2
======

- reset gameTaskString on disconnect

v5.1.1
======

- use new x86 webrtc node module for testclient release archive on travis CI

v5.1.0
======

- create a JS bundle of `faf-ice-adapter` on travis CI
- create a testclient release archive on travis CI

v5.0.2
======

- disconnect peers on game exit
- fix datachannel_open status

v5.0.1
======

- make JSON parsing more compatible
- cleanup

v5.0.0
======

- first nodejs-based release

v4.0.0
======

- refactor to use one stream per peer and a single agent

v3.0.0
======

- created `faf-ice-testserver` and `faf-ice-testclient`

v2.0.0
======

- include uPnP and port range options


v1.0.0
======

- first libnice based release
