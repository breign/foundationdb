################################################################################
# Helper Functions
################################################################################

function(install_symlink)
  set(options "")
  set(one_value_options COMPONENT FROM TO)
  set(multi_value_options)
  cmake_parse_arguments(SYM "${options}" "${one_value_options}" "${multi_value_options}" "${ARGN}")

  if(NOT SYM_COMPONENT OR NOT SYM_FROM OR NOT SYM_TO)
    message(FATA_ERROR "Invalid call to install_symlink")
  endif()
  get_filename_component(dest_dir ${SYM_TO} DIRECTORY)
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${dest_dir})" COMPONENT ${SYM_COMPONENT})
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${SYM_FROM} ${SYM_TO})" COMPONENT ${SYM_COMPONENT})
  install(CODE "message(\"-- Created symlink: ${SYM_FROM} -> ${SYM_TO}\")")
endfunction()
function(install_mkdir)
  set(options "")
  set(one_value_options COMPONENT NAME)
  set(multi_value_options)
  cmake_parse_arguments(MK "${options}" "${one_value_options}" "${multi_value_options}" "${ARGN}")
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${MK_NAME})" COMPONENT ${MK_COMPONENT})
  install(CODE "message(\"-- Created directory: ${MK_NAME}\")")
endfunction()

if(NOT INSTALL_LAYOUT)
  if(WIN32)
    set(DEFAULT_INSTALL_LAYOUT "WIN")
  else()
    set(DEFAULT_INSTALL_LAYOUT "STANDALONE")
  endif()
endif()
set(INSTALL_LAYOUT "${DEFAULT_INSTALL_LAYOUT}"
  CACHE STRING "Installation directory layout. Options are: TARGZ (as in tar.gz installer), WIN, STANDALONE, RPM, DEB, OSX")

set(DIR_LAYOUT ${INSTALL_LAYOUT})
if(DIR_LAYOUT MATCHES "TARGZ")
  set(DIR_LAYOUT "STANDALONE")
endif()

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

set(CPACK_PACKAGE_CHECKSUM SHA256)

set(FDB_CONFIG_DIR "etc/foundationdb")
if("${LIB64}" STREQUAL "TRUE")
  set(LIBSUFFIX 64)
else()
  set(LIBSUFFIX "")
endif()
set(FDB_LIB_NOSUFFIX "lib")
if(DIR_LAYOUT MATCHES "STANDALONE")
  set(FDB_LIB_DIR "lib${LIBSUFFIX}")
  set(FDB_LIBEXEC_DIR "${FDB_LIB_DIR}")
  set(FDB_BIN_DIR "bin")
  set(FDB_SBIN_DIR "sbin")
  set(FDB_INCLUDE_INSTALL_DIR "include")
  set(FDB_PYTHON_INSTALL_DIR "${FDB_LIB_DIR}/python2.7/site-packages/fdb")
  set(FDB_SHARE_DIR "share")
elseif(DIR_LAYOUT MATCHES "WIN")
  set(CPACK_GENERATOR "ZIP")
  set(FDB_CONFIG_DIR "etc")
  set(FDB_LIB_DIR "lib")
  set(FDB_LIB_NOSUFFIX "lib")
  set(FDB_LIBEXEC_DIR "bin")
  set(FDB_SHARE_DIR "share")
  set(FDB_BIN_DIR "bin")
  set(FDB_SBIN_DIR "bin")
  set(FDB_INCLUDE_INSTALL_DIR "include")
  set(FDB_PYTHON_INSTALL_DIR "${FDB_LIB_DIR}/python2.7/site-packages/fdb")
  set(FDB_SHARE_DIR "share")
elseif(DIR_LAYOUT MATCHES "OSX")
  set(CPACK_GENERATOR productbuild)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/")
  set(FDB_CONFIG_DIR "usr/local/etc/foundationdb")
  set(FDB_LIB_DIR "usr/local/lib")
  set(FDB_LIB_NOSUFFIX "usr/local/lib")
  set(FDB_LIBEXEC_DIR "usr/local/libexec")
  set(FDB_BIN_DIR "usr/local/bin")
  set(FDB_SBIN_DIR "usr/local/libexec")
  set(FDB_INCLUDE_INSTALL_DIR "usr/local/include")
  set(FDB_PYTHON_INSTALL_DIR "Library/Python/2.7/site-packages/fdb")
  set(FDB_SHARE_DIR "usr/local/share")
else()
  if(DIR_LAYOUT MATCHES "RPM")
    set(CPACK_GENERATOR RPM)
  else()
    # DEB
    set(CPACK_GENERATOR "DEB")
    set(LIBSUFFIX "")
  endif()
  set(CMAKE_INSTALL_PREFIX "/")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/")
  set(FDB_CONFIG_DIR "etc/foundationdb")
  set(FDB_LIB_DIR "usr/lib${LIBSUFFIX}")
  set(FDB_LIBEXEC_DIR ${FDB_LIB_DIR})
  set(FDB_BIN_DIR "usr/bin")
  set(FDB_SBIN_DIR "usr/sbin")
  set(FDB_INCLUDE_INSTALL_DIR "usr/include")
  set(FDB_PYTHON_INSTALL_DIR "${FDB_LIB_DIR}/python2.7/site-packages/fdb")
  set(FDB_SHARE_DIR "usr/share")
endif()

if(INSTALL_LAYOUT MATCHES "OSX")
  set(FDBMONITOR_INSTALL_LOCATION "${FDB_LIBEXEC_DIR}")
else()
  set(FDBMONITOR_INSTALL_LOCATION "${FDB_LIB_NOSUFFIX}/foundationdb")
endif()


################################################################################
# Version information
################################################################################

string(REPLACE "." ";" FDB_VERSION_LIST ${FDB_VERSION_PLAIN})
list(GET FDB_VERSION_LIST 0 FDB_MAJOR)
list(GET FDB_VERSION_LIST 1 FDB_MINOR)
list(GET FDB_VERSION_LIST 2 FDB_PATCH)

################################################################################
# General CPack configuration
################################################################################

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "foundationdb")
set(CPACK_PACKAGE_VENDOR "FoundationDB <fdb-dist@apple.com>")
set(CPACK_PACKAGE_VERSION_MAJOR ${FDB_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${FDB_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${FDB_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_SOURCE_DIR}/packaging/description)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "FoundationDB is a scalable, fault-tolerant, ordered key-value store with full ACID transactions.")
set(CPACK_PACKAGE_ICON ${CMAKE_SOURCE_DIR}/packaging/foundationdb.ico)
set(CPACK_PACKAGE_CONTACT "The FoundationDB Community")
set(CPACK_COMPONENT_server_DEPENDS clients)
if (INSTALL_LAYOUT MATCHES "OSX")
  # MacOS needs a file exiension for the LICENSE file
  set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/packaging/osx/resources/conclusion.rtf)
  set(CPACK_PRODUCTBUILD_RESOURCES_DIR ${CMAKE_SOURCE_DIR}/packaging/osx/resources)
  configure_file(${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_BINARY_DIR}/License.txt COPYONLY)
  set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_BINARY_DIR}/License.txt)
else()
  set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)
  set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
endif()

################################################################################
# Configuration for RPM
################################################################################

if(INSTALL_LAYOUT MATCHES "RPM")
  set(CPACK_RPM_server_USER_FILELIST
    "%config(noreplace) /etc/foundationdb/foundationdb.conf"
    "%attr(0700,foundationdb,foundationdb) /var/log/foundationdb"
    "%attr(0700, foundationdb, foundationdb) /var/lib/foundationdb")
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/sbin"
    "/usr/share/java"
    "/usr/lib64/python2.7"
    "/usr/lib64/python2.7/site-packages"
    "/var"
    "/var/log"
    "/var/lib"
    "/lib"
    "/lib/systemd"
    "/lib/systemd/system"
    "/etc/rc.d/init.d")
  set(CPACK_RPM_server_DEBUGINFO_PACKAGE ON)
  set(CPACK_RPM_clients_DEBUGINFO_PACKAGE ON)
  set(CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX /usr/src)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
  set(CPACK_RPM_clients_PRE_INSTALL_SCRIPT_FILE
    ${CMAKE_SOURCE_DIR}/packaging/rpm/scripts/preclients.sh)
  set(CPACK_RPM_clients_POST_INSTALL_SCRIPT_FILE
    ${CMAKE_SOURCE_DIR}/packaging/rpm/scripts/postclients.sh)
  set(CPACK_RPM_server_PRE_INSTALL_SCRIPT_FILE
    ${CMAKE_SOURCE_DIR}/packaging/rpm/scripts/preserver.sh)
  set(CPACK_RPM_server_POST_INSTALL_SCRIPT_FILE
    ${CMAKE_SOURCE_DIR}/packaging/rpm/scripts/postserver.sh)
  set(CPACK_RPM_server_PRE_UNINSTALL_SCRIPT_FILE
    ${CMAKE_SOURCE_DIR}/packaging/rpm/scripts/preunserver.sh)
  set(CPACK_RPM_server_PACKAGE_REQUIRES
    "foundationdb-clients = ${FDB_MAJOR}.${FDB_MINOR}.${FDB_PATCH}")
  #set(CPACK_RPM_java_PACKAGE_REQUIRES
  #  "foundationdb-clients = ${FDB_MAJOR}.${FDB_MINOR}.${FDB_PATCH}")
  set(CPACK_RPM_python_PACKAGE_REQUIRES
    "foundationdb-clients = ${FDB_MAJOR}.${FDB_MINOR}.${FDB_PATCH}")
  install_mkdir(NAME "var/log/foundationdb" COMPONENT server)
  install_mkdir(NAME "var/lib/foundationdb" COMPONENT server)
endif()

################################################################################
# Configuration for DEB
################################################################################

if(INSTALL_LAYOUT MATCHES "DEB")
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_DEBIAN_PACKAGE_SECTION "database")
  set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)

  set(CPACK_DEBIAN_SERVER_PACKAGE_DEPENDS "adduser, libc6 (>= 2.12), python (>= 2.6), foundationdb-clients (= ${FDB_VERSION})")
  set(CPACK_DEBIAN_CLIENTS_PACKAGE_DEPENDS "adduser, libc6 (>= 2.12)")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://www.foundationdb.org")
  set(CPACK_DEBIAN_CLIENTS_PACKAGE_CONTROL_EXTRA
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-CLIENTS/postinst)
  set(CPACK_DEBIAN_SERVER_PACKAGE_CONTROL_EXTRA
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-server/conffiles
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-server/preinst
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-server/postinst
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-server/prerm
    ${CMAKE_SOURCE_DIR}/packaging/deb/DEBIAN-foundationdb-server/postrm)
endif()

################################################################################
# MacOS configuration
################################################################################

if(INSTALL_LAYOUT MATCHES "OSX")
  set(CPACK_PREFLIGHT_SERVER_SCRIPT ${CMAKE_SOURCE_DIR}/packaging/osx/scripts-server/preinstall)
  set(CPACK_POSTFLIGHT_SERVER_SCRIPT ${CMAKE_SOURCE_DIR}/packaging/osx/scripts-server/postinstall)
  set(CPACK_POSTFLIGHT_CLIENTS_SCRIPT ${CMAKE_SOURCE_DIR}/packaging/osx/scripts-server/preinstall)
  install(PROGRAMS ${CMAKE_SOURCE_DIR}/packaging/osx/uninstall-FoundationDB.sh
    DESTINATION "usr/local/foundationdb"
    COMPONENT clients)
  install(FILES ${CMAKE_SOURCE_DIR}/packaging/osx/com.foundationdb.fdbmonitor.plist
    DESTINATION "Library/LaunchDaemons"
    COMPONENT server)
endif()

################################################################################
# Server configuration
################################################################################

string(RANDOM LENGTH 8 description1)
string(RANDOM LENGTH 8 description2)
set(CLUSTER_DESCRIPTION1 ${description1} CACHE STRING "Cluster description")
set(CLUSTER_DESCRIPTION2 ${description2} CACHE STRING "Cluster description")

if(NOT WIN32)
  if(INSTALL_LAYOUT MATCHES "OSX")
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/osx/foundationdb.conf.new
      DESTINATION ${FDB_CONFIG_DIR}
      COMPONENT server)
  else()
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/foundationdb.conf
      DESTINATION ${FDB_CONFIG_DIR}
      COMPONENT server)
  endif()
  install(FILES ${CMAKE_SOURCE_DIR}/packaging/argparse.py
    DESTINATION "${FDB_LIB_NOSUFFIX}/foundationdb"
    COMPONENT server)
  install(FILES ${CMAKE_SOURCE_DIR}/packaging/make_public.py
    DESTINATION "${FDB_LIB_NOSUFFIX}/foundationdb"
    COMPONENT server)
else()
  install(FILES ${CMAKE_BINARY_DIR}/fdb.cluster
    DESTINATION "etc"
    COMPONENT server)
endif()
if((INSTALL_LAYOUT MATCHES "RPM") OR (INSTALL_LAYOUT MATCHES "DEB"))
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/packaging/foundationdb
    ${CMAKE_BINARY_DIR}/packaging/rpm)
  install(
    DIRECTORY ${CMAKE_BINARY_DIR}/packaging/foundationdb
    DESTINATION "var/log"
    COMPONENT server)
  install(
    DIRECTORY ${CMAKE_BINARY_DIR}/packaging/foundationdb
    DESTINATION "var/lib"
    COMPONENT server)
  execute_process(
    COMMAND pidof systemd
    RESULT_VARIABLE IS_SYSTEMD
    OUTPUT_QUIET
    ERROR_QUIET)
  install(FILES ${CMAKE_SOURCE_DIR}/packaging/rpm/foundationdb.service
    DESTINATION "lib/systemd/system"
    COMPONENT server)
  if(INSTALL_LAYOUT MATCHES "RPM")
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/packaging/rpm/foundationdb-init
      DESTINATION "etc/rc.d/init.d"
      RENAME "foundationdb"
      COMPONENT server)
  else()
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/packaging/deb/foundationdb-init
      DESTINATION "etc/init.d"
      RENAME "foundationdb"
      COMPONENT server)
  endif()
endif()