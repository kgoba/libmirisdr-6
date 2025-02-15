# Copyright 2012 OSMOCOM Project
#
# This file is part of MiriSDR
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

option(SANITIZE "Sanitize addresses (check for buffer overflows)" OFF)
if (SANITIZE)
    message (STATUS "Building with address sanitisation enabled")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
else (SANITIZE)
    message (STATUS "Building with address sanitisation  disabled, use -DSANITIZE=ON to enable")
endif (SANITIZE)

########################################################################
# Setup library
########################################################################
add_library(mirisdr_shared SHARED
    libmirisdr.c
    reg.c
    adc.c
    convert.c
    async.c
    devices.c
    gain.c
    hard.c
    streaming.c
    soft.c
    sync.c
)

target_link_libraries(mirisdr_shared
    ${LIBUSB_LIBRARIES}
)

set_target_properties(mirisdr_shared PROPERTIES DEFINE_SYMBOL "mirisdr_EXPORTS")
set_target_properties(mirisdr_shared PROPERTIES OUTPUT_NAME mirisdr)
set_target_properties(mirisdr_shared PROPERTIES SOVERSION ${MAJOR_VERSION})
set_target_properties(mirisdr_shared PROPERTIES VERSION ${LIBVER})

add_library(mirisdr_static STATIC
    libmirisdr.c
    reg.c
    adc.c
    convert.c
    async.c
    devices.c
    gain.c
    hard.c
    streaming.c
    soft.c
    sync.c
)

if(WIN32)
add_library(libgetopt_static STATIC
    getopt/getopt.c
)
endif()

target_link_libraries(mirisdr_static
    ${LIBUSB_LIBRARIES}
)

set_property(TARGET mirisdr_static APPEND PROPERTY COMPILE_DEFINITIONS "mirisdr_STATIC" )

if(NOT WIN32)
# Force same library filename for static and shared variants of the library
set_target_properties(mirisdr_static PROPERTIES OUTPUT_NAME mirisdr)
endif()

########################################################################
# Setup libraries used in executables
########################################################################
add_library(convenience_static STATIC
    convenience/convenience.c
)
target_include_directories(convenience_static
  PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
add_library(libgetopt_static STATIC
    getopt/getopt.c
)
target_link_libraries(convenience_static
    mirisdr
)
endif()

########################################################################
# Build utility
########################################################################
add_executable(miri_sdr miri_sdr.c)
add_executable(miri_fm miri_fm.c)
add_executable(miri_power miri_power.c)
set(INSTALL_TARGETS mirisdr_shared mirisdr_static miri_sdr miri_fm miri_power)

target_link_libraries(miri_sdr mirisdr_shared convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(miri_fm mirisdr_shared convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(miri_power mirisdr_shared convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

if(UNIX)
target_link_libraries(miri_fm m)
target_link_libraries(miri_power m)
endif()

if(WIN32)
target_link_libraries(miri_sdr libgetopt_static)
target_link_libraries(miri_fm libgetopt_static)
target_link_libraries(miri_power libgetopt_static)
set_property(TARGET miri_sdr APPEND PROPERTY COMPILE_DEFINITIONS "mirisdr_STATIC" )
set_property(TARGET miri_fm APPEND PROPERTY COMPILE_DEFINITIONS "mirisdr_STATIC" )
set_property(TARGET miri_power APPEND PROPERTY COMPILE_DEFINITIONS "mirisdr_STATIC" )
endif()

########################################################################
# Install built library files & utilities
########################################################################
install(TARGETS ${INSTALL_TARGETS}
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)
