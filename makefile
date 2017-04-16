CC = gcc
CC_FLAGS = -O -Wall -Werror -DLINUX

threadlessweb:
	$(CC) $(CC_FLAGS) threadlessweb.c twexample.c -o threadlessweb

clean:
	rm -f *.o threadlessweb

