TARGET = blinker

all:
	gcc -g -O0 blinker.c -o $(TARGET)


clean:
	rm -rf $(TARGET)



