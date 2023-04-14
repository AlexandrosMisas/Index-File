CC      	:= gcc
LIB     	:= ../../lib/
INCLUDE 	:= ../../include
TESTS   	:= ../../tests
MODULES 	:= ../modules
BUILD_DIR   := ../../build
BIN_DIR     := ../../bin
CFLAGS	  	:= -I$(INCLUDE) -Wall -Werror

ifeq ($(DEBUG), ON)
	CFLAGS += -g3
endif


EXEC := heap_test
OBJS := heap_file.o record.o dl_list.o heap_test.o

OBJ := $(patsubst %,$(BIN_DIR)/%,$(OBJS))



$(BUILD_DIR)/$(EXEC): $(OBJ)
	@$(MAKE) build_dir
	@$(CC) -L $(LIB) -Wl,-rpath,$(LIB) -o $@ $^ -lbf


$(BIN_DIR)/%.o: %.c
	@$(MAKE) bin_dir
	$(CC) $(CFLAGS) -c $< -o $@  


$(BIN_DIR)/%.o: $(MODULES)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@ 


$(BIN_DIR)/%.o: $(TESTS)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)


bin_dir:  
	@if [ ! -d $(BIN_DIR) ]; then \
		mkdir -p $(BIN_DIR); \
	fi


build_dir:  
	@if [ ! -d $(BUILD_DIR) ]; then \
		mkdir -p $(BUILD_DIR); \
	fi