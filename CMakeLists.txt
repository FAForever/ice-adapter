cmake_minimum_required(VERSION 2.8)

project(ice-adapter CXX C)

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

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused")

if(NOT PKGCONFIG_FOUND)
  find_package(PkgConfig REQUIRED)
endif()
pkg_search_module(PJPROJECT REQUIRED libpjproject)
pkg_search_module(LIBMICROHTTPD REQUIRED libmicrohttpd)

include_directories(${PJPROJECT_INCLUDE_DIRS})
include_directories(${LIBMICROHTTPD_INCLUDE_DIRS})

add_executable(icetest
  devtest/icetest.cpp)

target_link_libraries(icetest
  ${PJPROJECT_LIBRARIES}
  )

add_executable(pjsua2_demo
  devtest/pjsua2_demo.cpp)

target_link_libraries(pjsua2_demo
  ${PJPROJECT_LIBRARIES}
  )

#find_package(cppnetlib REQUIRED)
#include_directories(${cppnetlib_INCLUDE_DIRS})

add_executable(myice1
  devtest/myice1.cpp
  devtest/PjIceStreamTransport.cpp
  devtest/PjIceGlobal.cpp
  devtest/HttpTest.cpp
  )

target_link_libraries(myice1
  ${PJPROJECT_LIBRARIES}
  ${LIBMICROHTTPD_LIBRARIES}
  cppnetlib-uri
  )

#add_subdirectory(libretest)
add_subdirectory(libnicetest)
