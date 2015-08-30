SRCS=main.c
default:
	cc -c ${SRCS}
	cc -lc -lnetgraph -o main *.o

clean:
	rm -f *.o main
