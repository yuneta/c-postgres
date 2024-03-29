##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yuneta-postgres C)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/output)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
  option(SHOWNOTES "Show preprocessor notes" OFF)

  if(CMAKE_COMPILER_IS_GNUCC)
    # GCC specific debug options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3 -ggdb3 -gdwarf-2")
    set(AVOID_VERSION -avoid-version)
  endif(CMAKE_COMPILER_IS_GNUCC)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

# find_package(PostgreSQL REQUIRED)

include_directories(/yuneta/development/output/include)

##############################################
#   Source
##############################################

set (SRCS
    src/yuneta_postgres_register.c

    # Services

    # Protocols

    # Mixin uv-gobj
    src/c_postgres.c
)


set (HDRS
    src/yuneta_postgres.h
    src/yuneta_postgres_version.h
    src/yuneta_postgres_register.h

    # Services

    # Protocols

    # Mixin uv-gobj
    src/c_postgres.h
)


##############################################
#   Lib
##############################################
add_library(yuneta-postgres ${SRCS} ${HDRS})

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS yuneta-postgres
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)

##############################################
#   sub-projects
##############################################
