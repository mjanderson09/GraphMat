
DIST_PRIMITIVES_PATH=GraphMatDistributedPrimitives
include $(DIST_PRIMITIVES_PATH)/Make.inc

#CXX=icpc
CXX=mpiicpc
CXX_OPTIONS=-qopenmp -std=c++11 -Isrc -I$(DIST_PRIMITIVES_PATH) 


ifeq (${debug}, 1)
  CXX_OPTIONS += -O0 -g -D__DEBUG $(GPFLAGS)
else
  CXX_OPTIONS += -O3 -ipo $(GPFLAGS)
endif

#CXX_OPTIONS += -xHost

ifeq (${timing}, 1)
  CXX_OPTIONS += -D__TIMING
else
endif

CXX_OPTIONS += -I/home/hadoop/hadoop/include -L/home/hadoop/hadoop/lib/native -L/usr/lib/jvm/jre-1.7.0-openjdk-1.7.0.85-2.6.1.2.el7_1.x86_64/lib/amd64/server -ljvm -lhdfs 

SRCDIR=src
BINDIR=bin

SOURCES=$(SRCDIR)/PageRank.cpp $(SRCDIR)/Degree.cpp $(SRCDIR)/BFS.cpp $(SRCDIR)/SGD.cpp $(SRCDIR)/TriangleCounting.cpp $(SRCDIR)/SSSP.cpp $(SRCDIR)/Delta.cpp $(SRCDIR)/PageRank_spark.cpp

DEPS=$(SRCDIR)/SPMV.cpp $(SRCDIR)/Graph.cpp $(SRCDIR)/GraphProgram.cpp $(SRCDIR)/SparseVector.cpp $(SRCDIR)/GraphMatRuntime.cpp $(DIST_PRIMITIVES_PATH)/src/layouts.h $(DIST_PRIMITIVES_PATH)/src/graphpad.h

EXE=$(BINDIR)/PageRank $(BINDIR)/PageRank_spark $(BINDIR)/IncrementalPageRank $(BINDIR)/BFS $(BINDIR)/SSSP #$(BINDIR)/TriangleCounting $(BINDIR)/SGD $(BINDIR)/DS


all: $(EXE) graph_converter
	
graph_converter: graph_utils/graph_convert.cpp
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/graph_converter graph_utils/graph_convert.cpp

$(BINDIR)/PageRank: $(DEPS) $(MULTINODEDEPS) $(SRCDIR)/PageRank.cpp $(SRCDIR)/Degree.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/PageRank $(SRCDIR)/PageRank.cpp  

$(BINDIR)/PageRank_spark: $(DEPS) $(MULTINODEDEPS) $(SRCDIR)/PageRank_spark.cpp $(SRCDIR)/Degree.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/PageRank_spark $(SRCDIR)/PageRank_spark.cpp  

$(BINDIR)/IncrementalPageRank: $(DEPS) $(MULTINODEDEPS) $(SRCDIR)/IncrementalPageRank.cpp $(SRCDIR)/Degree.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/IncrementalPageRank $(SRCDIR)/IncrementalPageRank.cpp 

$(BINDIR)/BFS: $(DEPS) $(SRCDIR)/BFS.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/BFS $(SRCDIR)/BFS.cpp 

$(BINDIR)/SGD: $(DEPS) $(SRCDIR)/SGD.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/SGD $(SRCDIR)/SGD.cpp

$(BINDIR)/TriangleCounting: $(DEPS) $(SRCDIR)/TriangleCounting.cpp
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/TriangleCounting $(SRCDIR)/TriangleCounting.cpp

$(BINDIR)/SSSP: $(DEPS) $(MULTINODEDEPS) $(SRCDIR)/SSSP.cpp 
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/SSSP $(SRCDIR)/SSSP.cpp 

$(BINDIR)/DS: $(DEPS) $(SRCDIR)/Delta.cpp
	$(CXX) $(CXX_OPTIONS) -o $(BINDIR)/DS $(SRCDIR)/Delta.cpp

clean:
	rm $(EXE) bin/graph_converter 
