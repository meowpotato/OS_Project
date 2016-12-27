all: 70 80 90

70:	70.c
	gcc -g -o 70 70.c

80:	80.c
	gcc -g -o 80 80.c

90:	90.c
	gcc -g -o 90 90.c -lpthread -lm

clean: 
	rm -f *.o 70 80 90
