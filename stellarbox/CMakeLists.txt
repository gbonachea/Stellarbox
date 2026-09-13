cmake_minimum_required(VERSION 3.16)

project(stellarbox
    VERSION 1.0
    DESCRIPTION "Stellarbox window manager (CMake build)"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -O2)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

# ---------------------------------------------------------------------------
# Dependencies via pkg-config
# ---------------------------------------------------------------------------
pkg_check_modules(X11        REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(XCB        REQUIRED IMPORTED_TARGET xcb)
pkg_check_modules(XEXT       REQUIRED IMPORTED_TARGET xext)
pkg_check_modules(XFT        REQUIRED IMPORTED_TARGET xft)
pkg_check_modules(XRANDR     REQUIRED IMPORTED_TARGET xrandr)
pkg_check_modules(XRENDER    REQUIRED IMPORTED_TARGET xrender)
pkg_check_modules(XSHAPE     QUIET    IMPORTED_TARGET xshape)
pkg_check_modules(GLIB2      REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(XML2       REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(CAIRO      REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(PANGO      REQUIRED IMPORTED_TARGET pango)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(PANGOXFT   REQUIRED IMPORTED_TARGET pangoxft)
pkg_check_modules(RSVG       QUIET    IMPORTED_TARGET librsvg-2.0)

set(PKG_TARGETS
    PkgConfig::X11
    PkgConfig::XCB
    PkgConfig::XEXT
    PkgConfig::XFT
    PkgConfig::XRANDR
    PkgConfig::XRENDER
    PkgConfig::GLIB2
    PkgConfig::XML2
    PkgConfig::CAIRO
    PkgConfig::PANGO
    PkgConfig::PANGOCAIRO
    PkgConfig::PANGOXFT)

# xshape has no .pc file on most systems; XShape lives in libXext.
# Use the module when available and fall back to Xext otherwise.
if(XSHAPE_FOUND)
    list(APPEND PKG_TARGETS PkgConfig::XSHAPE)
endif()

# librsvg is optional (same as upstream autotools); enables SVG icons.
if(RSVG_FOUND)
    list(APPEND PKG_TARGETS PkgConfig::RSVG)
    set(USE_LIBRSVG 1)
endif()

# ---------------------------------------------------------------------------
# Generated headers (replaces the autotools config.h / version.h step)
# ---------------------------------------------------------------------------
include(CheckIncludeFile)

# Upstream defines all HAVE_* macros on the compiler command line (automake
# DEFS), so they reach every TU even when config.h is not included.
set(OB_CHECK_HEADERS
    fcntl.h grp.h locale.h pwd.h signal.h stdio.h stdlib.h string.h
    sys/select.h sys/stat.h sys/types.h sys/wait.h unistd.h)
set(OB_HAVE_DEFS)
foreach(hdr ${OB_CHECK_HEADERS})
    string(MAKE_C_IDENTIFIER "${hdr}" VID)
    string(TOUPPER "HAVE_${VID}" VAR)
    check_include_file("${hdr}" "${VAR}")
    if(${VAR})
        list(APPEND OB_HAVE_DEFS "${VAR}=1")
    endif()
endforeach()

add_compile_definitions(${OB_HAVE_DEFS})

# Package identity (autotools used to pass these on the command line too).
add_compile_definitions(
    PACKAGE=\"stellarbox\"
    PACKAGE_NAME=\"stellarbox\"
    PACKAGE_TARNAME=\"stellarbox\"
    PACKAGE_VERSION=\"${PROJECT_VERSION}\"
    PACKAGE_STRING=\"stellarbox\ ${PROJECT_VERSION}\"
    PACKAGE_BUGREPORT=\"http://bugzilla.icculus.org\"
    VERSION=\"${PROJECT_VERSION}\")

# X11 extensions present via the linked pkg-config modules.
add_compile_definitions(SHAPE=1 XRANDR=1 XKB=1 SYNC=1)
if(RSVG_FOUND)
    add_compile_definitions(USE_LIBRSVG=1)
endif()

configure_file(cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(OB_VERSION 3.7)
set(OBT_MAJOR_VERSION 3)
set(OBT_MINOR_VERSION 6)
set(OBT_MICRO_VERSION 5)
set(RR_MAJOR_VERSION 3)
set(RR_MINOR_VERSION 6)
set(RR_MICRO_VERSION 33)

set(OBT_VERSION "${OBT_MAJOR_VERSION}.${OBT_MINOR_VERSION}.${OBT_MICRO_VERSION}")
set(RR_VERSION "${RR_MAJOR_VERSION}.${RR_MINOR_VERSION}.${RR_MICRO_VERSION}")

configure_file(version.h.in        ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
configure_file(obt/version.h.in    ${CMAKE_CURRENT_BINARY_DIR}/obt/version.h @ONLY)
configure_file(obrender/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/obrender/version.h @ONLY)
configure_file(cmake/obt-3.5.pc.in ${CMAKE_CURRENT_BINARY_DIR}/obt-3.5.pc @ONLY)
configure_file(cmake/obrender-3.5.pc.in ${CMAKE_CURRENT_BINARY_DIR}/obrender-3.5.pc @ONLY)

# ---------------------------------------------------------------------------
# Sources: all .c/.h from openbox/ and obrender/ (plus obt/, required by both)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE OBT_SOURCES   CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/obt/*.c)
file(GLOB       OBRENDER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/obrender/*.c)
file(GLOB_RECURSE OPENBOX_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openbox/*.c)

list(FILTER OBT_SOURCES EXCLUDE REGEX ".*(_unittest|unittest_base)\\.c$|/tests/.*")
list(FILTER OBRENDER_SOURCES EXCLUDE REGEX ".*/test\\.c$")

# ---------------------------------------------------------------------------
# obrender (render library)
# ---------------------------------------------------------------------------
add_library(obrender STATIC ${OBRENDER_SOURCES})
target_compile_features(obrender PUBLIC c_std_11)
target_compile_definitions(obrender PRIVATE
    DEFAULT_THEME=\"Clia\"
    "G_LOG_DOMAIN=\"ObRender\"")
target_include_directories(obrender PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(obrender PRIVATE ${PKG_TARGETS})

# ---------------------------------------------------------------------------
# obt (obt library: xml, keyboard, display, ...)
# ---------------------------------------------------------------------------
add_library(obt STATIC ${OBT_SOURCES})
target_compile_features(obt PUBLIC c_std_11)
target_compile_definitions(obt PUBLIC
    "LOCALEDIR=\"${CMAKE_INSTALL_FULL_DATADIR}/locale\""
    "DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}\""
    "CONFIGDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg\"")
target_compile_definitions(obt PRIVATE "G_LOG_DOMAIN=\"Obt\"")
target_include_directories(obt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(obt PUBLIC ${PKG_TARGETS})

# ---------------------------------------------------------------------------
# stellarbox executable
# ---------------------------------------------------------------------------
add_executable(stellarbox ${OPENBOX_SOURCES})

target_include_directories(stellarbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(stellarbox PRIVATE
    "G_LOG_DOMAIN=\"Stellarbox\""
    "LOCALEDIR=\"${CMAKE_INSTALL_FULL_DATADIR}/locale\""
    "DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}\""
    "CONFIGDIR=\"${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg\"")

target_link_libraries(stellarbox PRIVATE obrender obt ${PKG_TARGETS})

install(TARGETS stellarbox RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---------------------------------------------------------------------------
# Desarrollo: instalar librerías, cabeceras y .pc (obt/obrender, como el
# upstream autotools) para los consumidores pkg-config (p.ej. stellarbox-obconf)
# ---------------------------------------------------------------------------
install(TARGETS obt obrender ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/obt-3.5.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/obrender-3.5.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(DIRECTORY obrender/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openbox/obrender
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY obt/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openbox/obt
        FILES_MATCHING PATTERN "*.h"
        PATTERN "internal.h" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/obrender/version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openbox/obrender)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/obt/version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openbox/obt)