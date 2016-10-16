#include <sigc++/sigc++.h>

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

#include <chrono>
#include <condition_variable>
#include <giomm.h>
#include <glibmm.h>
#include <iostream>
#include <memory>

namespace
{

Glib::RefPtr<Glib::MainLoop> loop;

Glib::ustring
socket_address_to_string(const Glib::RefPtr<Gio::SocketAddress>& address)
{
  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(address);
  if (!isockaddr)
    return Glib::ustring();

  auto inet_address = isockaddr->get_address();
  auto str = inet_address->to_string();
  auto the_port = isockaddr->get_port();
  auto res = Glib::ustring::compose("%1:%2", str, the_port);
  return res;
}

static bool source_ready(Glib::IOCondition /*condition*/)
{
  loop->quit();
  return false;
}

} // end anonymous namespace

int
main(int argc, char* argv[])
{
  Glib::RefPtr<Gio::Socket> socket, new_socket, recv_socket;
  Glib::RefPtr<Gio::SocketAddress> address;

  Gio::init();

  int port = 7777;
  bool non_blocking = true;

  loop = Glib::MainLoop::create();

  auto socket_type = Gio::SOCKET_TYPE_STREAM;
  //auto socket_family = Gio::SOCKET_FAMILY_IPV6;
  auto socket_family = Gio::SOCKET_FAMILY_IPV4;

  try
  {
    socket = Gio::Socket::create(socket_family, socket_type, Gio::SOCKET_PROTOCOL_DEFAULT);
  }
  catch (const Gio::Error& error)
  {
    std::cerr << Glib::ustring::compose("%1: %2\n", argv[0], error.what());
    return 1;
  }

  socket->set_blocking(false);

  auto src_address =
    Gio::InetSocketAddress::create(Gio::InetAddress::create_any(socket_family), 5362);
  try
  {
    socket->bind(src_address, false);
  }
  catch (const Gio::Error& error)
  {
    std::cerr << Glib::ustring::compose("Can't bind socket: %1\n", error.what());
    return 1;
  }

  try
  {
    socket->listen();
  }
  catch (const Gio::Error& error)
  {
    std::cerr << Glib::ustring::compose("Can't listen on socket: %1\n", error.what());
    return 1;
  }

  std::cout << Glib::ustring::compose("listening on port %1...\n", 5362) << std::endl;

  Gio::signal_socket().connect([](Glib::IOCondition condition){
    std::cout << "onconnect" << std::endl;

    new_socket = socket->accept(cancellable);
    return false;
  }, socket, Glib::IO_IN);

  loop->run();

  return 0;
}
