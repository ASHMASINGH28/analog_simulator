cmake_minimum_required(VERSION 3.10)
project(mna_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

add_executable(mna_sim
    src/main.cpp
    src/Netlist.cpp
    src/MnaSolver.cpp
)

target_include_directories(mna_sim PRIVATE include)
target_link_libraries(mna_sim PRIVATE Eigen3::Eigen)
