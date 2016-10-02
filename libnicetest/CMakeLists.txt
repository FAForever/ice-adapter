
add_executable(libnicetest
  LibNiceTest.cpp
  IceAgent.cpp
  IceStream.cpp
  )

if(NOT PKGCONFIG_FOUND)
  find_package(PkgConfig REQUIRED)
endif()
pkg_search_module(NICE REQUIRED nice)
pkg_search_module(SOUP REQUIRED libsoup-2.4)
pkg_search_module(JSON REQUIRED json-glib-1.0)

include_directories(${NICE_INCLUDE_DIRS})
include_directories(${SOUP_INCLUDE_DIRS})
include_directories(${JSON_INCLUDE_DIRS})

target_link_libraries(libnicetest
  ${NICE_LIBRARIES}
  )

add_executable(ice-adapter
  HttpClient.cpp
  HttpServer.cpp
  IceAdapter.cpp
  IceAgent.cpp
  IceStream.cpp
  main.cpp
  )

target_link_libraries(ice-adapter
  ${NICE_LIBRARIES}
  ${SOUP_LIBRARIES}
  ${JSON_LIBRARIES}
  )
