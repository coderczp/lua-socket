packagesocket.so: lpackagesocket.c PackageSocket.cpp
	g++ -g -o2 -Wall -llua --shared -std=c++11 -o $@ $^


.PHONY: clean

clean:
	rm -rf packagesocket.so	
