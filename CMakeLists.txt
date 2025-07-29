cmake_minimum_required(VERSION 3.10)

project(volume-notifier C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)

# Generate the compile_commands.json file. src: https://mzhang.io/posts/2022-03-03-clangd-in-nix/
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

add_executable(volume-notifier
    src/main.c
    src/notification.c
    src/volume_listener.c
)

target_include_directories(volume-notifier PRIVATE
    include
    ${PIPEWIRE_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)
target_link_libraries(volume-notifier PRIVATE
    ${PIPEWIRE_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    m
)
target_compile_definitions(volume-notifier PRIVATE
    ${PIPEWIRE_CFLAGS_OTHER}
    ${LIBNOTIFY_CFLAGS_OTHER}
)
