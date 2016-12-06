cmake_minimum_required(VERSION 3.0)

project(faf-ice-adapter
  LANGUAGES CXX C
  )

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" ICE_ADAPTER_COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" ICE_ADAPTER_COMPILER_SUPPORTS_CXX0X)
if(ICE_ADAPTER_COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(ICE_ADAPTER_COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused")

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

if(NOT PKGCONFIG_FOUND)
  find_package(PkgConfig REQUIRED)
endif()

if(NOT (NICE_LIBRARIES AND NICE_INCLUDE_DIRS))
  pkg_search_module(NICE REQUIRED nice)
endif()

if(NOT (JSON_LIBRARIES AND JSON_INCLUDE_DIRS))
  pkg_search_module(JSON REQUIRED jsoncpp)
endif()

if(NOT (GIOMM_LIBRARIES AND GIOMM_INCLUDE_DIRS))
  pkg_search_module(GIOMM REQUIRED giomm-2.4)
endif()

find_package(Boost REQUIRED COMPONENTS log program_options thread system)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${NICE_INCLUDE_DIRS})
include_directories(${JSON_INCLUDE_DIRS})
include_directories(${GIOMM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(docs SOURCES README.md LICENSE)

add_library(fafice
  STATIC
    GPGNetServer.cpp
    GPGNetMessage.cpp
    IceAdapter.cpp
    IceAdapterOptions.cpp
    IceAgent.cpp
    JsonRpcProtocol.cpp
    JsonRpcServer.cpp
    logging.cpp
    PeerRelay.cpp
    Socket.cpp
    TcpServer.cpp
    TcpSession.cpp
)
target_compile_definitions(fafice PRIVATE FAF_VERSION_STRING="${FAF_VERSION_STRING}")
target_link_libraries(fafice
  ${GIOMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${JSON_LIBRARIES}
  ${NICE_LIBRARIES}
)

add_executable(faf-ice-adapter
  main.cpp
)

target_link_libraries(faf-ice-adapter
  fafice
)

target_compile_definitions(faf-ice-adapter PRIVATE BOOST_SYSTEM_NO_DEPRECATED)

if(NOT MINGW)
  target_link_libraries(fafice pthread)
  target_compile_definitions(fafice PUBLIC BOOST_LOG_DYN_LINK)
endif()

# Strip exe to reduce file size
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_custom_command(TARGET faf-ice-adapter
    POST_BUILD
    COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-adapter>"
    COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-adapter>"
    )
endif()

option(FAF_BUILD_TEST "Create test UI" OFF)
if(FAF_BUILD_TEST)
  find_package(Qt5Widgets REQUIRED)
  find_package(Qt5Network REQUIRED)

  if(MINGW)
    pkg_search_module(QT5W REQUIRED Qt5Widgets)
    pkg_search_module(QT5N REQUIRED Qt5Network)
    link_directories(${QT5W_LIBRARY_DIRS})
    link_directories(${QT5N_LIBRARY_DIRS})
    include_directories(${Qt5Widgets_INCLUDE_DIRS})
    include_directories(${Qt5Network_INCLUDE_DIRS})
    set(faf_test_lib_libs
      Qt5::QWindowsIntegrationPlugin
      libQt5PlatformSupport.a
      ${QT5W_LIBRARIES}
      ${QT5N_LIBRARIES})
  else()
    set(faf_test_lib_libs
      Qt5::Widgets
      Qt5::Network)
  endif()

  set(CMAKE_AUTOMOC ON)
  qt5_wrap_ui(FAF_TEST_UIS_H test/Testclient.ui)
  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  add_executable(faf-ice-testclient
      ${FAF_TEST_UIS_H}
      test/GPGNetClient.cpp
      test/JsonRpcClient.cpp
      test/Testclient.cpp
      test/Testclient.ui
  )
  target_link_libraries(faf-ice-testclient
    fafice
    ${faf_test_lib_libs}
  )
  if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET faf-ice-testclient
      POST_BUILD
      COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-testclient>"
      COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-testclient>"
      )
  endif()

  target_compile_definitions(faf-ice-testclient PRIVATE QT_NO_KEYWORDS)

  add_executable(faf-ice-testserver
      test/Testserver.cpp
  )
  target_link_libraries(faf-ice-testserver
    fafice
  )
  if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET faf-ice-testserver
      POST_BUILD
      COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-testserver>"
      COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-testserver>"
      )
  endif()
endif()
