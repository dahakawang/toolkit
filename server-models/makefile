SOURCES=$(shell ls *.cc)
OBJECTS=$(SOURCES:.cc=.o)
FLAGS=--std=c++11 -g #-O3
CXX=g++-4.8

all: prog


prog: $(OBJECTS)
	$(CXX) $(FLAGS) -o prog $(OBJECTS)

$(OBJECTS): %.o:%.cc
	$(CXX) $(FLAGS) -c -o $@ $<

clean:
	rm *.o
