ADD_EXECUTABLE (frontend front_end.cpp front_end.hpp structure.h)
#target_link_libraries(frontend ${CIMENGINE_NAME}-static pthread dl m jsoncpp_object jsoncpp_static JsonCpp::JsonCpp)
target_link_libraries(frontend ${CIMENGINE_NAME}-static jsoncpp_object jsoncpp_static JsonCpp::JsonCpp)

find_package(jsoncpp CONFIG REQUIRED) #use jsoncppConfig.cmake to config the cmake

#TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_SOURCE_DIR}/source")
TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_BINARY_DIR}/source")
TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_SOURCE_DIR}/source/operator/prototype")
TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_SOURCE_DIR}/tools")
#TARGET_INCLUDE_DIRECTORIES (frontend PRIVATE "${CMAKE_SOURCE_DIR}/examples/common")
