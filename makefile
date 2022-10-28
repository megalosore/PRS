all: udpserveur udpclient

udpserveur: udpserveur.c
	gcc -Wall udpserveur.c -o udpserveur
	
udpclient: udpclient.c
	gcc -Wall udpclient.c -o udpclient
