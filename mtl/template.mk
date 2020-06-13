##
##  Template makefile for Standard, Profile, Debug, Release, and Release-static versions
##
##    eg: "make rs" for a statically linked release version.
##        "make d"  for a debug version (no optimizations).
##        "make"    for the standard version (optimized, but with debug information and assertions active)

PWD        = $(shell pwd)
EXEC      ?= $(notdir $(PWD))

CSRCS      = $(wildcard $(PWD)/*.cc) 
DSRCS      = $(foreach dir, $(DEPDIR), $(filter-out $(MROOT)/$(dir)/Main.cc, $(wildcard $(MROOT)/$(dir)/*.cc)))
CHDRS      = $(wildcard $(PWD)/*.h)
COBJS      = $(CSRCS:.cc=.o) $(DSRCS:.cc=.o)

PCOBJS     = $(addsuffix p,  $(COBJS))
DCOBJS     = $(addsuffix d,  $(COBJS))
RCOBJS     = $(addsuffix r,  $(COBJS))


CXX       ?= g++
CFLAGS    ?= -Wall -Wno-parentheses $(CMD_CFLAGS)
LFLAGS    ?= -Wall

COPTIMIZE ?= -O3

CFLAGS    += -I$(MROOT) -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS
LFLAGS    += -lz

.PHONY : s p d r rs clean 

s:	$(EXEC)
p:	$(EXEC)_profile
d:	$(EXEC)_debug
r:	$(EXEC)_release
rs:	$(EXEC)_static

libs:	lib$(LIB)_standard.a
libp:	lib$(LIB)_profile.a
libd:	lib$(LIB)_debug.a
libr:	lib$(LIB)_release.a

## Compile options
%.o:			CFLAGS +=$(COPTIMIZE) -g -D DEBUG
%.op:			CFLAGS +=$(COPTIMIZE) -pg -g -D NDEBUG
%.od:			CFLAGS +=-O0 -g -D DEBUG
%.or:			CFLAGS +=$(COPTIMIZE) -g -D NDEBUG

## Link options
$(EXEC):		LFLAGS += -g
$(EXEC)_profile:	LFLAGS += -g -pg
$(EXEC)_debug:		LFLAGS += -g
#$(EXEC)_release:	LFLAGS += ...
$(EXEC)_static:		LFLAGS += --static

## Dependencies
$(EXEC):		$(COBJS)
$(EXEC)_profile:	$(PCOBJS)
$(EXEC)_debug:		$(DCOBJS)
$(EXEC)_release:	$(RCOBJS)
$(EXEC)_static:		$(RCOBJS)cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

project(MapleSat)

#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)

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

#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISAT_LIB_SOURCES
    utils/Options.cc
    utils/System.cc
    core/Solver.cc
    simp/SimpSolver.cc)

add_library(minisat-lib-static STATIC ${MINISAT_LIB_SOURCES})
add_library(minisat-lib-shared SHARED ${MINISAT_LIB_SOURCES})

target_link_libraries(minisat-lib-shared ${ZLIB_LIBRARY})
target_link_libraries(minisat-lib-static ${ZLIB_LIBRARY})

add_executable(minisat_core core/Main.cc)
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

if(STATIC_BINARIES)
  target_link_libraries(minisat_core minisat-lib-static)
  target_link_libraries(minisat_simp minisat-lib-static)
else()
  target_link_libraries(minisat_core minisat-lib-shared)
  target_link_libraries(minisat_simp minisat-lib-shared)
endif()

set_target_properties(minisat-lib-static PROPERTIES OUTPUT_NAME "maplesat")
set_target_properties(minisat-lib-shared
  PROPERTIES
    OUTPUT_NAME "maplesat"
    VERSION ${MINISAT_VERSION}
    SOVERSION ${MINISAT_SOVERSION})

set_target_properties(minisat_simp       PROPERTIES OUTPUT_NAME "maplesat")

#--------------------------------------------------------------------------------------------------
# Installation targets:

install(TARGETS minisat-lib-static minisat-lib-shared minisat_core minisat_simp
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY minisat/mtl minisat/utils minisat/core minisat/simp
        DESTINATION include/minisat
        FILES_MATCHING PATTERN "*.h")


lib$(LIB)_standard.a:	$(filter-out */Main.o,  $(COBJS))
lib$(LIB)_profile.a:	$(filter-out */Main.op, $(PCOBJS))
lib$(LIB)_debug.a:	$(filter-out */Main.od, $(DCOBJS))
lib$(LIB)_release.a:	$(filter-out */Main.or, $(RCOBJS))


## Build rule
%.o %.op %.od %.or:	%.cc
	@echo Compiling: $(subst $(MROOT)/,,$@) $(CFLAGS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

## Linking rules (standard/profile/debug/release)
$(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static:
	@echo Linking: "$@ ( $(foreach f,$^,$(subst $(MROOT)/,,$f)) )"
	@$(CXX) $^ $(LFLAGS) -o $@

## Library rules (standard/profile/debug/release)
lib$(LIB)_standard.a lib$(LIB)_profile.a lib$(LIB)_release.a lib$(LIB)_debug.a:
	@echo Making library: "$@ ( $(foreach f,$^,$(subst $(MROOT)/,,$f)) )"
	@$(AR) -rcsv $@ $^

## Library Soft Link rule:
libs libp libd libr:
	@echo "Making Soft Link: $^ -> lib$(LIB).a"
	@ln -sf $^ lib$(LIB).a

## Clean rule
clean:
	@rm -f $(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static \
	  $(COBJS) $(PCOBJS) $(DCOBJS) $(RCOBJS) *.core depend.mk 

## Make dependencies
depend.mk: $(CSRCS) $(CHDRS)
	@echo Making dependencies
	@$(CXX) $(CFLAGS) -I$(MROOT) \
	   $(CSRCS) -MM | sed 's|\(.*\):|$(PWD)/\1 $(PWD)/\1r $(PWD)/\1d $(PWD)/\1p:|' > depend.mk
	@for dir in $(DEPDIR); do \
	      if [ -r $(MROOT)/$${dir}/depend.mk ]; then \
		  echo Depends on: $${dir}; \
		  cat $(MROOT)/$${dir}/depend.mk >> depend.mk; \
	      fi; \
	  done

-include $(MROOT)/mtl/config.mk
-include depend.mk
