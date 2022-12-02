all: udpserveur

udpserveur: udpserveur.c
	gcc -Wall udpserveur.c -o udpserveur -lpthread
