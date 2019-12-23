BIN = web_proxy
CPP = g++

all: web_proxy.o
	$(CPP) -o $(BIN) web_proxy.o -L ../lib -lasync -levent -lpthread

%.o: %.cpp
	$(CPP) -c $< -o $@ -I../include

clean:
	rm *.o $(BIN)
