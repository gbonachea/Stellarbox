cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)

project(stellarbox-obconf)

find_program(SED_PROGRAM sed)

include(GNUInstallDirs)

# Minimum Versions
set(GLIB_MINIMUM_VERSION "2.50.0")
set(QT_MINIMUM_VERSION "6.4.0")

set(SBOBCONF_MAJOR_VERSION 0)
set(SBOBCONF_MINOR_VERSION 16)
set(SBOBCONF_PATCH_VERSION 6)
set(SBOBCONF_VERSION ${SBOBCONF_MAJOR_VERSION}.${SBOBCONF_MINOR_VERSION}.${SBOBCONF_PATCH_VERSION})
add_definitions("-DSBOBCONF_VERSION=\"${SBOBCONF_VERSION}\"")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt6LinguistTools ${QT_MINIMUM_VERSION} REQUIRED)

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(OPENBOX REQUIRED
  obrender-3.5
  obt-3.5
)

message(STATUS "Building with Qt${Qt6Core_VERSION}")

# add src subdirectory
add_subdirectory(src)

# install an icon for the application
install(
  FILES data/stellarbox.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps"
)