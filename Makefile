# Example Makefile for ROSE users
# This makefile is provided as an example of how to use ROSE 

# Location of root directory for ROSE
ROSE_ROOT_LOC = $(ROSE_ROOT)

# Location of install directory after "make install"
ROSE_INSTALL_DIR = $(ROSE_ROOT_LOC)/install

# Location of include directory after "make install"
ROSE_INCLUDE_DIR = $(ROSE_INSTALL_DIR)/include

# Location of bin directory after "make install"
ROSE_BIN_DIR = $(ROSE_INSTALL_DIR)/bin

# Location of library directory after "make install"
ROSE_LIB_DIR = $(ROSE_ROOT_LOC)/install/lib

# Location of Boost include directory
#BOOST_CPPFLAGS = -pthread -I$(ROSE_ROOT_LOC)/BOOST_INSTALL/include
BOOST_CPPFLAGS = -pthread -I/usr/include/boost

CC                    = gcc
CXX                   = g++
CPPFLAGS              = 
CXXFLAGS              =  -std=c++11 -g
LDFLAGS               = 

ROSE_LIBS = $(ROSE_LIB_DIR)/librose.la

# Example suffix rule for more experienced makefile users
#.C.o:
#	g++ -c -I$(ROSE_INCLUDE_DIR) -o $@ $(@:.o=.C)


# Default make rule to use
all: AutoTile GenerateTiledBenchmarks

AutoTile.lo:	AutoTile.C
	/bin/sh $(ROSE_BIN_DIR)/libtool --mode=compile $(CXX) $(CXXFLAGS)  $(CPPFLAGS) -I$(ROSE_INCLUDE_DIR) -I$(ROSE_INCLUDE_DIR)/rose $(BOOST_CPPFLAGS) -c -o AutoTile.lo AutoTile.C

AutoTile: AutoTile.lo
	/bin/sh $(ROSE_BIN_DIR)/libtool --mode=link $(CXX) $(CXXFLAGS) $(LDFLAGS) -o AutoTile AutoTile.lo $(ROSE_LIBS)

GenerateTiledBenchmarks.lo:	GenerateTiledBenchmarks.C
	/bin/sh $(ROSE_BIN_DIR)/libtool --mode=compile $(CXX) $(CXXFLAGS)  $(CPPFLAGS) -I$(ROSE_INCLUDE_DIR) -I$(ROSE_INCLUDE_DIR)/rose $(BOOST_CPPFLAGS) -c -o GenerateTiledBenchmarks.lo GenerateTiledBenchmarks.C

GenerateTiledBenchmarks: GenerateTiledBenchmarks.lo
	/bin/sh $(ROSE_BIN_DIR)/libtool --mode=link $(CXX) $(CXXFLAGS) $(LDFLAGS) -o GenerateTiledBenchmarks GenerateTiledBenchmarks.lo $(ROSE_LIBS)

# Rule used by make installcheck to verify correctness of installed libraries
# check:
# 	./AutoTile testCode.C
# 	./GenerateTiledBenchmarks testCode.C

clean:
	rm AutoTile AutoTile.lo GenerateTiledBenchmarks GenerateTiledBenchmarks.lo
