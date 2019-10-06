All:
	g++ AcDc.c -g -o AcDc
debug:
	g++ AcDc.c -g -DDEBUG -o AcDc
clean:
	rm AcDc
