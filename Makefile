LIB := C:/Code/d3d11/libs
INCLUDE = C:/Code/d3d11/include
LINK_LIB_PATHS := $(foreach path,$(LIB),-L$(path) )
INCLUDE_PATH := $(foreach path,$(INCLUDE), -I$(path))
SRC = renderer.cpp decoder.cpp encoder.cpp test.cpp  
OBJS = $(SRC:.cpp=.o)
LIBS = $(LINK_LIB_PATHS)-lSDL2 -lavformat -lavcodec -lswscale -lavutil -luser32 -ldxgi -ld3d11
test: $(OBJS)
	g++ $(OBJS) -o $@ $(LIBS)
%.o: %.cpp
	g++ --std=c++11 -g -c -Wall -Wextra $< $(INCLUDE_PATH)

debug:
	@echo $(LINK_LIB_PATHS)

clean:
	rm -f *.o
	rm -f *.exe