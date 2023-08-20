a.out: vkinit.o x.o util.o 
#	cc -o a.out vk.o -lX11 -lvulkan task/libtask.a -lXfixes
#	cc -o a.out vk.o x.o -lSDL2 -lvulkan
	cc -o a.out vkinit.o x.o util.o -lvulkan -lX11 -lXfixes
#	cc -o a.out glinit.o x.o util.c -lGL -lX11 -lXfixes -lEGL
task/libtask.a: task
	cd task && mk && cd ..

%.o: %.c
	cc -c $stem.c -o $stem.o
clean:V:
	rm -f *.o
nuke: clean
	rm -f a.out

