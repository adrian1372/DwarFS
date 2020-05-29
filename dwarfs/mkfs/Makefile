.PHONY: all clean

cpp := mkfs.cpp
out := mkfs.dwarfs

all:
	g++ $(cpp) -o $(out)
	touch image

clean:
	rm $(out)
	rm image
