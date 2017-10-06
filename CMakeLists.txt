cmake_minimum_required(VERSION 3.0)

project(faf-ice-adapter
  LANGUAGES CXX C
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
  .travis.yml
  TODO
  )

add_library(fafice
  logging.cpp
  IceAdapter.cpp
  IceAdapterOptions.cpp
  IceTest.cpp
  JsonRpcServer.cpp
  GPGNetServer.cpp
  GPGNetMessage.cpp
  PeerRelay.cpp
  PeerRelayObservers.cpp
)
target_compile_definitions(fafice PRIVATE
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
  )
endif()

add_executable(faf-ice-adapter
  main.cpp
)

target_link_libraries(faf-ice-adapter
  fafice
)
