cmake_minimum_required (VERSION 3.1)

project (mytest)

set (CMAKE_CXX_STANDARD 17)

file(GLOB HEADER_FILES *.h)
file(GLOB SOURCE_FILES *.cpp)

add_executable(mytest ${SOURCE_FILES} ${HEADER_FILES})

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_RELEASE_LIBS ON)
find_package(Boost 1.75.0 REQUIRED COMPONENTS fiber context)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(mytest
  Boost::fiber
  Threads::Threads
)
