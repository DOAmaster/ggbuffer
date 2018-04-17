all: ggbuffer

ggbuffer: ggbuffer.cpp
	g++ ggbuffer.cpp -Wall -Wextra -oggbuffer -lX11 -lGL -lGLU -lm -lrt

clean:
	rm -f ggbuffer
	rm -f *.o

