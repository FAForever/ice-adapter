cmake_minimum_required(VERSION 2.8)

project(snowflake CXX C)

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

if(NOT PKGCONFIG_FOUND)
  find_package(PkgConfig REQUIRED)
endif()
pkg_search_module(NICE REQUIRED nice)
pkg_search_module(JSON REQUIRED jsoncpp)

include_directories(${NICE_INCLUDE_DIRS})
include_directories(${JSON_INCLUDE_DIRS})

#add_executable(ice-adapter
#  HttpClient.cpp
#  HttpServer.cpp
#  IceAdapter.cpp
#  IceAgent.cpp
#  main.cpp
#  )

#target_link_libraries(ice-adapter
#  ${NICE_LIBRARIES}
#  ${SOUP_LIBRARIES}
#  ${JSON_LIBRARIES}
#  )

add_executable(libnice_example
  libnice_example.c
  )
target_link_libraries(libnice_example
  ${NICE_LIBRARIES}
  )

find_package(Boost COMPONENTS iostreams system REQUIRED)

add_executable(jsonstreamtest
  jsonstreamtest.cpp
  JsonRpcServer.cpp
  )
target_link_libraries(jsonstreamtest
  ${Boost_LIBRARIES}
  ${JSON_LIBRARIES}
  pthread
  )

add_executable(boost_example
  boost_example.cpp
  )
target_link_libraries(boost_example
  ${Boost_LIBRARIES}
  pthread
  )

add_custom_target(docs SOURCES README.md LICENSE)
