cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_ARCHITECTURES "arm64")

project(PIMCOM)

find_package(jsoncpp CONFIG REQUIRED) #use jsoncppConfig.cmake to config the cmake

file(GLOB_RECURSE SRC "${CMAKE_CURRENT_SOURCE_DIR}/backend/*.cpp")
file(GLOB_RECURSE Header "${CMAKE_CURRENT_SOURCE_DIR}/backend/*.h")
list(APPEND SRC ${Header})

add_executable(${PROJECT_NAME}  ${SRC})

target_link_libraries(PIMCOM PRIVATE jsoncpp_object) #Lib