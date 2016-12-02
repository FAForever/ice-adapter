#pragma once

#include <string>
#include <array>
#include <vector>

#include <giomm.h>

#include "GPGNetMessage.h"

namespace faf
{

class GPGNetServer;

/**
 * Handles a GPGNet connection and decodes its insanity.
 * A GPGNet packet looks like this:
 *
 * 0           1           2           3           4
 * +-----------+-----------+-----------+-----------+
 * |               Header length, h                |
 * +-----------+-----------+-----------+-----------+
 * | Header ....
 * +-----------+-----------+-----------+-----------+
 * |                 Chunk count, c                |
 * +-----------+-----------+-----------+-----------+
 * | Chunks ....
 * +-----------+-----------+-----------+-----------+
 *
 * A chunk (which represents one Lua value) looks like this:
 *
 * 0           1           2           3           4           5
 * +-----------+-----------+-----------+-----------+-----------+
 * |   type    |                    length                     |
 * +-----------+-----------+-----------+-----------+-----------+
 * | Payload ....
 * +-----------+-----------+-----------+-----------+-----------+
 *
 * For type == 0 (int), payload is absent and the length field carries the value.
 * For type == 1 (string), payload is a utf-8 string of length-many bytes.
 * TODO: Aren't there other types?
 */
class GPGNetConnection
{
public:
  GPGNetConnection(GPGNetServer* server,
                   Glib::RefPtr<Gio::Socket> socket);
virtual ~GPGNetConnection();

  void sendMessage(GPGNetMessage const& msg);

protected:
  bool onRead(Glib::IOCondition condition);
  void parseMessages();
  void debugOutputMessage(GPGNetMessage const& msg);

  Glib::RefPtr<Gio::Socket> mSocket;
  std::array<char, 4096> mReadBuffer;
  std::vector<char> mMessage;

  /* points to the end of mBuffer */
  GPGNetServer* mServer;
};

}
