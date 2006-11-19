default:
	gcc -g -Wall -Werror dynparam_preallocate.c dynparam.c audiolock.c zynjacku.c slv2.c log.c `pkg-config --cflags --libs libslv2 jack` -o zynjacku

clean:
	-rm zynjacku
