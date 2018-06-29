cmake_minimum_required(VERSION 3.0)

project(faf-ice-adapter
  LANGUAGES CXX
  )

set(CMAKE_CXX_STANDARD 17)

set(WEBRTC_INCLUDE_DIRS "" CACHE PATH "libwebrtc include dir")
if(NOT EXISTS "${WEBRTC_INCLUDE_DIRS}")
  message(FATAL_ERROR "please specify WEBRTC_INCLUDE_DIRS to point at the include path of the libwebrtc package from https://github.com/FAForever/libwebrtc/releases")
endif()

set(WEBRTC_LIBRARIES "" CACHE FILEPATH "libwebrtc lib")
if(NOT EXISTS "${WEBRTC_LIBRARIES}")
  message(FATAL_ERROR "please specify WEBRTC_LIBRARIES to point at the lib file of the libwebrtc package from https://github.com/FAForever/libwebrtc/releases")
endif()

if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT WIN32)
  message(FATAL_ERROR "the ice-adapter must be built using clang on linux")
endif()

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


include_directories("${WEBRTC_INCLUDE_DIRS}")
include_directories("${WEBRTC_INCLUDE_DIRS}/webrtc")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(docs
  SOURCES
  README.md
  LICENSE
  appveyor.yml
  CHANGELOG.md
  .travis.yml
  )

add_library(fafice
  GPGNetServer.cpp
  GPGNetMessage.cpp
  IceAdapter.cpp
  IceAdapterOptions.cpp
  JsonRpc.cpp
  JsonRpcServer.cpp
  logging.cpp
  PeerConnectivityChecker.cpp
  PeerRelay.cpp
  PeerRelayObservers.cpp
  Timer.cpp
  trim.cpp
)
target_compile_definitions(fafice PUBLIC
  FAF_VERSION_STRING="${FAF_VERSION_STRING}";
  WEBRTC_BUILD_LIBEVENT)
target_link_libraries(fafice
  ${WEBRTC_LIBRARIES}
)

if(NOT WIN32)
  target_compile_definitions(fafice PUBLIC WEBRTC_LINUX WEBRTC_POSIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -pthread -fno-rtti")
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
  main.cpp
)
target_link_libraries(faf-ice-adapter
  fafice
)


add_library(faficetest
  test/GPGNetClient.cpp
  test/JsonRpcClient.cpp
  test/Process.cpp
  test/Pingtracker.cpp
  )
target_link_libraries(faficetest
  fafice
)

add_executable(faf-ice-testserver
  test/TestServer.cpp
)
target_link_libraries(faf-ice-testserver
  fafice
  faficetest
)

add_executable(faf-ice-testclient
  test/TestClient.cpp
  )
target_link_libraries(faf-ice-testclient
  fafice
  faficetest
  )


add_executable(webrtctestsimple2peers
  test/WebrtcTestSimple2Peers.cpp
  )
target_link_libraries(webrtctestsimple2peers
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )

add_executable(webrtctest
  test/WebrtcTest.cpp
  )
target_link_libraries(webrtctest
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )

add_executable(webrtctestwithpingtracker
  test/WebrtcTestWithPingtracker.cpp
  )
target_link_libraries(webrtctestwithpingtracker
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )

add_executable(IceAdapterTest
  test/IceAdapterTest.cpp
  )
target_link_libraries(IceAdapterTest
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )

add_executable(GPGNetServerTest
  test/GPGNetServerTest.cpp
  )
target_link_libraries(GPGNetServerTest
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )

add_executable(PeerRelayTest
  test/PeerRelayTest.cpp
  )
target_link_libraries(PeerRelayTest
  fafice
  faficetest
  ${WEBRTC_LIBRARIES}
  )
