

#############################
# main tasks

all: jquery.c bixtags.h b 

# -msse2 is used to make floats more conformant to IEEE 754
COPTIONS=-g -msse2
WOPTIONS=-Wall -Wno-unused-variable -Wno-format-extra-args -Wno-unused-but-set-variable -Wno-unused-function -Wno-pointer-sign -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

b:;
		gcc $(COPTIONS) $(WOPTIONS) -o bchain $(CMDLINECFLAGS) maincoreserver.c expmem.c jsvar.c consensus.c common.c bix.c bchain.c coins.c exchange.c json.c jquery.c -lcrypto -lssl

#############################
# generated sources

jquery.c: jquery.js
		xxd -i jquery.js > jquery.c

bixtags.h:;
		bash generateBixTags.sh > bixtags.h

#############################
# start/stop installation, cleanup, etc

start:;
		(ulimit -c unlimited ; (./bchain core-00 & ./bchain core-01 & ./bchain core-02 & ./bchain core-03 & ./bchain core-04 & ./bchain core-05 & ) > currentlog.txt 2>&1) &

stop:;
		(killall -q bchain ; sleep 3; killall -9 -q bchain; sleep 2 )

restart: stop start

clean:	stop
		(cd ..; rm -f *.o core* *~ */*~ */*/*~ )
		(cd ../src; rm -f *.o core* o[0-9].txt bchain )
		rm -f jquery.c bixtags.h currentlog.txt currentsubmsgs.txt

blockchainclean: clean
		rm -r -f ../data/accounts ../data/blocks ../data/db ../data/status ../data/supportedmsgs ../data/accountspertoken

distclean: blockchainclean
		rm -f server.key server.crt

dist:	distclean
		(cd ../..; tar cfvz bix-blockchain-`date "+%Y-%m-%d"`.tgz bix-blockchain)

install: distclean
		 bash initaccounts.sh 6
		 openssl req -x509 -nodes -days 9999 -newkey rsa:4096 -keyout server.key -out server.crt -batch

run:	start
		tail -f currentlog.txt


# to install on remote machine. usage:  make scp d=remote@host.com
scp:	distclean
		scp -r ./*  $(d):bix-blockchain/src/

