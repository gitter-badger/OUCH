CXXFLAGS  += -Wall -fPIC -std=c++0x -O3 -DNDEBUG

SRC := \
	$(notdir $(wildcard *.c)) \
	$(notdir $(wildcard *.cc)) \
	$(notdir $(wildcard *.cxx)) \
	$(notdir $(wildcard *.cpp))

OBJ := \
	$(patsubst %.c, %.o,  $(filter %.c, $(SRC)))  \
	$(patsubst %.cc, %.o, $(filter %.cc, $(SRC))) \
	$(patsubst %.cxx, %.o, $(filter %.cxx, $(SRC))) \
	$(patsubst %.cpp, %.o,  $(filter %.cpp, $(SRC)))

DEPEND = $(notdir $(wildcard .depend))

all: .depend libouch.so libouch.a

.depend: $(SRC)
	$(CXX) $(CXXFLAGS) -w -MM $^ > $@

libouch.so: $(OBJ)
	$(CXX) -shared -o $@ $^ $(LDFLAGS) 

libouch.a: $(OBJ)
	ar rcs $@ $^

clean:
	rm -rf $(OBJ) .depend libouch.so libouch.a

ifneq ($(DEPEND),)
include .depend
endif

