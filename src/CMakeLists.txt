cmake_minimum_required (VERSION 2.6)

pkg_check_modules(GFAL2_PKG         REQUIRED gfal2>=2.4.0)
pkg_check_modules(GFAL_TRANSFER_PKG REQUIRED gfal_transfer>=2.4.0)
pkg_check_modules(GLIB2_PKG         REQUIRED glib-2.0)

string (REPLACE "." ";" GFAL2_VERSION_LIST ${GFAL2_PKG_VERSION})
list (GET GFAL2_VERSION_LIST 0 GFAL2_VERSION_MAJOR)
list (GET GFAL2_VERSION_LIST 1 GFAL2_VERSION_MINOR)

# Find Boost 1.41
set (Boost_NO_BOOST_CMAKE ON)
set (BOOST_INCLUDEDIR "/usr/include/boost141") # EPEL5
set (BOOST_LIBRARYDIR "/usr/lib${LIB_SUFFIX}/boost141/") # EPEL5
set (Boost_ADDITIONAL_VERSIONS "1.41" "1.41.0")

find_package(Boost 1.41 COMPONENTS thread REQUIRED)
find_package(XROOTD REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_definitions(${GFAL2_PKG_CFLAGS})
add_definitions(${GFAL_TRANSFER_PKG_CFLAGS})
add_definitions(${GLIB2_PKG_CFLAGS})
add_definitions("-DGFAL2_VERSION_MAJOR=${GFAL2_VERSION_MAJOR}" "-DGFAL2_VERSION_MINOR=${GFAL2_VERSION_MINOR}")

include_directories(${GFAL2_PKG_INCLUDE_DIRS}
                    ${GLIB2_PKG_INCLUDE_DIRS}
                    ${GFAL_TRANSFER_PKG_INCLUDE_DIRS}
                    ${XROOTD_INCLUDE_DIR})

file (GLOB src_xrootd "*.cpp")

add_library (gfal_plugin_xrootd SHARED ${src_xrootd})

target_link_libraries (gfal_plugin_xrootd ${Boost_LIBRARIES})
target_link_libraries (gfal_plugin_xrootd ${GFAL2_PKG_LIBRARIES})
target_link_libraries (gfal_plugin_xrootd gfal_transfer)
target_link_libraries (gfal_plugin_xrootd XrdClient XrdCl XrdPosix)

install(TARGETS gfal_plugin_xrootd
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR})


add_subdirectory (tests)
