cmake_minimum_required(VERSION 4.0)
project(TP_RESEAU C)

set(CMAKE_C_STANDARD 11)

add_executable(client client.c)
add_executable(serveur server.c)
