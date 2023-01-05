COMPILER = gcc
FILESYSTEM_FILES = my_fs.c

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -o my_fs `pkg-config fuse --cflags --libs`
	echo 'To Mount: ./my_fs -f [mount point]'

clean:
	rm ssfs
