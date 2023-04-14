MAKE 		+= --silent
BUILD_DIR 	= ./build
LIB 		= ./lib
HEAP_FILE 	= ./src/Heap_File
HASH_FILE 	= ./src/Hash_File
SHASH_FILE 	= ./src/SHash_File
EXEC_FILES 	= $(shell find $(BUILD_DIR) -type f -executable)
VAL_FLAGS 	:= valgrind  --leak-check=full --show-leak-kinds=all --track-origins=yes


all: heap_file hash_file shash_file

.PHONY: heap_file  hash_file shash_file

heap_file:
	@$(MAKE) -C $(HEAP_FILE)

hash_file:
	@$(MAKE) -C $(HASH_FILE)

shash_file:
	@$(MAKE) -C $(SHASH_FILE)


run:
	@for exec in $(EXEC_FILES); do \
		LD_LIBRARY_PATH=$(LIB) ./$$exec; \
	done

valgrind:
	@for exec in $(EXEC_FILES); do \
		LD_LIBRARY_PATH=$(LIB) $(VAL_FLAGS) ./$$exec; \
	done

gdb:
	@for exec in $(EXEC_FILES); do \
		LD_LIBRARY_PATH=$(LIB) gdb ./$$exec; \
	done


clean:
	@rm -rf *.db
	@$(MAKE) -C $(HEAP_FILE) clean