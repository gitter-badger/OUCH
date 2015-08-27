PREFIX = /user/local/

lib: 
	cd src; make

.PHONY: lib test clean

test: lib
	$(CXX) test/test.C -o $@.out -louch -Iinclude -Lsrc -pthread -std=c++0x -O3 -DNDEBUG

clean:
	rm -rf test.out;
	cd src; make clean

install: lib
	mkdir -p $(PREFIX)/include/ouch; cp -rfL include/* $(PREFIX)/include/ouch/
	mkdir -p $(PREFIX)/lib; cp -rf src/libouch* $(PREFIX)/lib
