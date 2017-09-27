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
  GPGNetServer.cpp
  GPGNetMessage.cpp
)
target_compile_definitions(fafice PRIVATE FAF_VERSION_STRING="${FAF_VERSION_STRING}")
target_link_libraries(fafice
  ${WEBRTC_LIBRARIES}
)

add_executable(faf-ice-adapter
  main.cpp
)

target_link_libraries(faf-ice-adapter
  fafice
)

target_compile_definitions(faf-ice-adapter PRIVATE BOOST_SYSTEM_NO_DEPRECATED)

if(NOT WIN32)
  target_link_libraries(fafice dl;SM;ICE;X11;Xext;rt)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_POSIX -D_GLIBCXX_USE_CXX11_ABI=0 -std=c++17 -pthread")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  target_compile_definitions(fafice PUBLIC WEBRTC_LINUX)
  target_compile_definitions(fafice PUBLIC WEBRTC_POSIX)
else()
  target_compile_definitions(fafice PUBLIC WEBRTC_WIN)
endif()

# Strip exe to reduce file size
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_custom_command(TARGET faf-ice-adapter
    POST_BUILD
    COMMAND ${CMAKE_STRIP} -s "$<TARGET_FILE:faf-ice-adapter>"
    COMMENT "Stripping executable $<TARGET_FILE_NAME:faf-ice-adapter>"
    )
endif()

