cmake_minimum_required(VERSION 3.5)

project(faf-ice-testclient
  LANGUAGES CXX
  )

find_package(PkgConfig REQUIRED)
pkg_search_module(JSON REQUIRED jsoncpp)
include_directories(${JSON_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
set(CMAKE_AUTOMOC ON)

if(MINGW)
  set(faf_test_libs
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    )
else()
  set(faf_test_libs
    Qt5::Widgets
    Qt5::Network
    pthread)
endif()

include_directories(${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Network_INCLUDE_DIRS})

add_executable(faf-ice-legacy-testserver
    Testserver.cpp
    JsonRpcProtocol.cpp
    JsonRpcQTcpServer.cpp
    JsonRpcQTcpSocket.cpp
    Socket.cpp
)
target_link_libraries(faf-ice-legacy-testserver
  ${faf_test_libs}
  ${JSON_LIBRARIES}
)
set_property(TARGET faf-ice-legacy-testserver PROPERTY CXX_STANDARD 11)
set_property(TARGET faf-ice-legacy-testserver PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_custom_command(TARGET faf-ice-legacy-testserver
    POST_BUILD
    COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-legacy-testserver>"
    COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-legacy-testserver>"
    )
endif()


qt5_wrap_ui(FAF_TEST_UIS_H Testclient.ui)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(faf-ice-testclient WIN32
    ${FAF_TEST_UIS_H}
    GPGNetClient.cpp
    GPGNetMessage.cpp
    IceCandidate.cpp
    JsonRpcProtocol.cpp
    JsonRpcQTcpServer.cpp
    JsonRpcQTcpSocket.cpp
    Pingtracker.cpp
    Socket.cpp
    Testclient.cpp
    Testclient.ui
)
target_link_libraries(faf-ice-testclient
  ${faf_test_libs}
  ${JSON_LIBRARIES}
)
set_property(TARGET faf-ice-testclient PROPERTY CXX_STANDARD 11)
set_property(TARGET faf-ice-testclient PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_custom_command(TARGET faf-ice-testclient
    POST_BUILD
    COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-testclient>"
    COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-testclient>"
    )
endif()
