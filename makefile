all: udpserveur

udpserveur: udpserveur.c
	gcc udpserveur.c -o udpserveur
