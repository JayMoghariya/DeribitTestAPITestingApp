cmake_minimum_required(VERSION 3.10)
project(DeribitTradingSystem)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find and include libcurl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Find and include Boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Find OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Include nlohmann/json
include_directories(include/json/include/)

include_directories(include)

# Include websocketpp (assuming it's in /external/websocketpp)
include_directories(external/websocketpp)

# Add executable
# Add executable
add_executable(DeribitTradingSystem src/main.cpp src/config.cpp)

# Link libraries
target_link_libraries(DeribitTradingSystem ${CURL_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto ${Boost_LIBRARIES} pthread)