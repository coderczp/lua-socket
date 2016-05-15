packagesocket.so: lpackagesocket.c 
	gcc -g -o2 -Wall -llua --shared -std=c99 -o $@ $^


.PHONY: clean

clean:
	rm -rf packagesocket.so	
