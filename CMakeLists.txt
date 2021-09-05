cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
project(cps_bundler VERSION 1.0
                    DESCRIPTION "CPS source god files bundler.\
                                 It will moves generated god files by eWam to directory categorizing each\
                                 class/module by it's bundle.\
                                 The categorizing will based on bundle-index.txt file"
                    LANGUAGES CXX)

add_executable(CpsBundler src/main.cpp)
