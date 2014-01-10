LIBS = `pkg-config --libs --cflags xcb`
OBJS = wm.o #display.o window.o
PROGRAM = wm 

all: $(PROGRAM)

$(PROGRAM) : $(OBJS)
	g++ -o wm $(OBJS) $(LIBS)

clean:
	rm $(OBJS)

%.o : %.cpp
	g++ -c -o $@ $< $(CFLAGS)
