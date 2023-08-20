a.out: vkinit.o x.o util.o
	cc -o a.out vkinit.o x.o util.o -lvulkan -lX11 -lXfixes

%.o: %.c
	cc -c $stem.c -o $stem.o
clean:V:
	rm -f *.o
nuke: clean
	rm -f a.out

