cmake_minimum_required(VERSION 3.1)

project(Slacken CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.9 CONFIG REQUIRED Widgets WebSockets Network NetworkAuth)

set(SLACKEN_SRCS 
	mainwindow.ui main.qrc
	main.cpp mainwindow.cpp slackclient.cpp 
)

add_executable(slacken ${SLACKEN_SRCS})
target_link_libraries(slacken PUBLIC Qt5::Widgets Qt5::WebSockets Qt5::Network Qt5::NetworkAuth)
## target_include_directories(slacken PUBLIC ${
