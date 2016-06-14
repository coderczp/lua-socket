packagesocket.so: lpackagesocket.c 
	gcc -g -o2 -Wall --shared -std=c99 -I./../lua/src -o $@ $^

.PHONY: clean

clean:
	rm -rf packagesocket.so	
