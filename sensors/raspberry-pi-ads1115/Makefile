.PHONY: clean

ads1115: ads1115.c
	gcc -g -Wall -Wextra -pedantic -std=c11 -D_DEFAULT_SOURCE -D_BSD_SOURCE -o ads1115 ads1115.c -lwiringPi -lm

clean:
	rm ads1115
