all: mazedemo
.phony: all

clean:
	rm -f *.o mazedemo
.phony: clean

CXXFLAGS := -std=gnu++11 -g -ggdb
CFLAGS := -std=gnu99 -g -ggdb
LDLIBS := -lopencv_core -lopencv_imgproc -lopencv_highgui

# needed because of C++
LINK.o = $(LINK.cc)

mazedemo: img_processing.o img_input.o mazedemo.o mazegen.o
