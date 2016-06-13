packagesocket.so: lpackagesocket.c 
	gcc -g -o2 -Wall --shared -std=c99 -I./../lua/src -llua -o $@ $^

.PHONY: clean

clean:
	rm -rf packagesocket.so	
