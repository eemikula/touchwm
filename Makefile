LIBS = `pkg-config --libs --cflags xcb` -pthread
OBJS = wm.o window.o screen.o
PROGRAM = wm 

all: $(PROGRAM)

$(PROGRAM) : $(OBJS)
	g++ -g -o wm $(OBJS) $(LIBS)

clean:
	rm $(OBJS) $(PROGRAM)

%.o : %.cpp
	g++ -c -o $@ $< $(CFLAGS)
