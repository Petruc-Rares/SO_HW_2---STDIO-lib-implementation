build: so_stdio.c
	gcc -fPIC -c so_stdio.c 
	gcc -shared so_stdio.o -o libso_stdio.so

clean:
	rm so_stdio.o