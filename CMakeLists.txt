cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# setting Compiler (settig compiler may affect things --see cmake documetation)
#set(CMAKE_CXX_COMPILER "/usr/bin/mpicxx")
#set(CMAKE_C_COMPILER "/usr/bin/mpicc")

project(MapleSat)
#set(CMAKE_CXX_STANDARD 11)
#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)
option(PROFILE "Profiling using mpiP" OFF)
#--------------------------------------------------------------------------------------------------


#set(CXX_COMPILER_COVERAGE_FLAGS "-g -fPIC")
if (PROFILE)
  set(CXX_LINKER_COVERAGE_FLAGS "-lbfd -ldl -lm -liberty -lunwind")
endif ()

#--------------------------------------------------------------------------------------------------
# Library version:

set(MINISAT_SOMAJOR   2)
set(MINISAT_SOMINOR   1)
set(MINISAT_SORELEASE 0)

# Compute VERSION and SOVERSION:
if (USE_SORELEASE)
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR}.${MINISAT_SORELEASE})
else()
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR})
endif()
set(MINISAT_SOVERSION ${MINISAT_SOMAJOR})

#--------------------------------------------------------------------------------------------------
# Dependencies:

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIR})
include_directories(${minisat_SOURCE_DIR})

#profiling dependency:
if (PROFILE)
#  include_directories("/home/lavleshm/Downloads/mpiP-3.4.1/include")
  link_directories("/home/lavleshm/Downloads/mpiP-3.4.1/lib")
endif ()


#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)


#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISAT_LIB_SOURCES
    utils/Options.cc
    utils/System.cc
    core/Solver.cc
    simp/SimpSolver.cc
   )

#include_directories(./scheduler)
#link_directories("/home/lavleshm/Simulator-MapleSat/scheduler")

add_library(minisat-lib-static STATIC ${MINISAT_LIB_SOURCES})
add_library(minisat-lib-shared SHARED ${MINISAT_LIB_SOURCES})

target_link_libraries(minisat-lib-shared ${ZLIB_LIBRARY})
target_link_libraries(minisat-lib-static ${ZLIB_LIBRARY})

#target_link_libraries(minisat-lib-shared ${sch-lib-shared})
#target_link_libraries(minisat-lib-static ${sch-lib-static})
include_directories(.)
#add_executable(minisat_core core/Main.cc)
add_executable(minisat_simp simp/Main.cc)

#............................................................................................

# Find MPI
find_package(MPI REQUIRED)

# Add executables
#add_executable(mainExec main.cpp)

# Link against MPI
target_link_libraries(minisat-lib-shared ${MPI_LIBRARIES})
target_link_libraries(minisat-lib-static ${MPI_LIBRARIES})

# Include MPI includes
include_directories(${MPI_INCLUDE_PATH})
if(MPI_COMPILE_FLAGS)
  set_target_properties(minisat-lib-shared PROPERTIES
          COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
  set_target_properties(minisat-lib-static PROPERTIES
          COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties(minisat-lib-shared PROPERTIES
          LINK_FLAGS "${MPI_LINK_FLAGS}")
  set_target_properties(minisat-lib-static PROPERTIES
          LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()


#............................................................................................
## Boost

set(Boost_USE_STATIC_LIBS        OFF)  # only find static libs
set(Boost_USE_DEBUG_LIBS         OFF) # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.60.0 REQUIRED COMPONENTS context)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(minisat-lib-static ${Boost_LIBRARIES})
  target_link_libraries(minisat-lib-shared ${Boost_LIBRARIES})
endif()

#............................................................................................
if(STATIC_BINARIES)
#  target_link_libraries(minisat_core minisat-lib-static)
  target_link_libraries(minisat_simp minisat-lib-static)

else()
#  target_link_libraries(minisat_core minisat-lib-shared)
  target_link_libraries(minisat_simp minisat-lib-shared)

#  target_link_libraries(minisat_core sch-lib-shared)
#  target_link_libraries(minisat_simp sch-lib-shared)

endif()

if (PROFILE)
#  target_link_libraries(minisat_core libmpiP.so)
  target_link_libraries(minisat_simp libmpiP.so)
endif ()

set_target_properties(minisat-lib-static PROPERTIES OUTPUT_NAME "maplesat")
set_target_properties(minisat-lib-shared
  PROPERTIES
    OUTPUT_NAME "maplesat"
    VERSION ${MINISAT_VERSION}
    SOVERSION ${MINISAT_SOVERSION})

set_target_properties(minisat_simp       PROPERTIES OUTPUT_NAME "maplesat")

#SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CXX_COMPILER_COVERAGE_FLAGS}")
if (PROFILE)
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${CXX_LINKER_COVERAGE_FLAGS}")
endif ()

#--------------------------------------------------------------------------------------------------
# Installation targets:

install(TARGETS minisat-lib-static minisat-lib-shared  minisat_simp
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY minisat/mtl minisat/utils minisat/core minisat/simp
        DESTINATION include/minisat
        FILES_MATCHING PATTERN "*.h")
