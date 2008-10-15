CC=		gcc

OBJS=		avs3a.o			\
		sercomm.o


avs3a:		$(OBJS)
		$(CC) -o avs3a $(OBJS)

$(OBJS):	avs3a.h sercomm.h

install:	avs3a
		cp avs3a /usr/local/bin

clean:
		rm -f *.o avs3a *~

backup:
		mkdir -p /usr/local/backups
		tar -czf /usr/local/backups/avs3a-`date +%Y-%m-%d-%H-%M`.tar.gz *.c *.h Makefile

