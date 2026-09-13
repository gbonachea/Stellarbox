cmake_minimum_required(VERSION 3.10)
project(stellarbox-desktop VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0 gio-2.0 gdk-pixbuf-2.0 cairo)

add_executable(stellarbox-desktop src/main.c)
target_include_directories(stellarbox-desktop PRIVATE ${GTK3_INCLUDE_DIRS})
target_compile_options(stellarbox-desktop PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(stellarbox-desktop PRIVATE ${GTK3_LIBRARIES})

install(TARGETS stellarbox-desktop RUNTIME DESTINATION bin)
install(DIRECTORY data/backgrounds DESTINATION share/stellarbox)
install(FILES data/stellarbox-desktop.desktop DESTINATION share/applications)