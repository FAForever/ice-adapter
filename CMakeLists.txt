cmake_minimum_required(VERSION 3.0)

project(faf-ice-adapter
  LANGUAGES CXX
  )

set(CMAKE_CXX_STANDARD 17)

if(NOT FAF_VERSION_STRING)
  find_package(Git)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags
      OUTPUT_VARIABLE FAF_VERSION_STRING
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      )
    string(STRIP ${FAF_VERSION_STRING} FAF_VERSION_STRING)
  else()
    set(FAF_VERSION_STRING  "unknown")
  endif()
endif()

include_directories(${WEBRTC_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(docs
  SOURCES
  README.md
  LICENSE
  appveyor.yml
  CHANGELOG.md
  )

add_library(fafice
  GPGNetClient.cpp
  GPGNetServer.cpp
  GPGNetMessage.cpp
  IceAdapterOptions.cpp
  JsonRpc.cpp
  JsonRpcClient.cpp
  JsonRpcServer.cpp
  logging.cpp
  PeerRelay.cpp
  PeerRelayObservers.cpp
  Timer.cpp
)
target_compile_definitions(fafice PUBLIC
  FAF_VERSION_STRING="${FAF_VERSION_STRING}";
  WEBRTC_BUILD_LIBEVENT)
target_link_libraries(fafice
  ${WEBRTC_LIBRARIES}
)

if(NOT WIN32)
  target_compile_definitions(fafice PUBLIC WEBRTC_LINUX WEBRTC_POSIX WEBRTC_BUILD_LIBEVENT)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0 -std=c++17 -pthread")
  target_link_libraries(fafice
    dl
    SM
    ICE
    X11
    Xext
    rt)
else() # we assume MSVC for now
  target_compile_definitions(fafice PUBLIC WEBRTC_WIN NOMINMAX)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  target_link_libraries(fafice
    winmm
    secur32
    msdmo
    dmoguids
    wmcodecdspuuid
    ws2_32
  )
endif()

add_executable(faf-ice-adapter
  IceAdapter.cpp
)

target_link_libraries(faf-ice-adapter
  fafice
)

add_executable(faf-ice-testserver
  TestServer.cpp
)

target_link_libraries(faf-ice-testserver
  fafice
)

add_executable(faf-ice-testclient
  TestClient.cpp
  Process.cpp
  ${testclient_srcs}
  )

target_link_libraries(faf-ice-testclient
  fafice
  )
