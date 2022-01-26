# vi: set tabstop=4 noautoindent:


VERSION = 0.1.0

#
#
SUBJ = ltictr_proxy 
#
all: $(SUBJ)

#
CC   = gcc
AR   = ar
TH   = touch

LIB_DIR = ../JunkBox_Lib
LIB_BSC_DIR = $(LIB_DIR)/Lib
LIB_EXT_DIR = $(LIB_DIR)/xLib

LIB_BSC = $(LIB_BSC_DIR)/libbasic.a
LIB_EXT = $(LIB_EXT_DIR)/libextend.a

#DEBUG = -DEBUG

CFLAGS  = -fPIC -W -Wall -DHAVE_CONFIG_H -DENABLE_SSL -I/usr/include -I$(LIB_DIR) -I$(LIB_BSC_DIR) -I$(LIB_EXT_DIR) $(DEBUG) 

SLIB = -L$(LIB_BSC_DIR) -L/usr/local/ssl/lib -lbasic -lssl -lm -lcrypto -lcrypt -lz
ELIB = -L$(LIB_EXT_DIR) -lextend
#
#
#

.h.c:
	$(TH) $@


.c.o:
	$(CC) $< $(CFLAGS) -c -O2 


#
#
ltictr_proxy: ltictr_proxy.o ltictr_api.o ltictr_child.o ltictr_xmlrpc.o ltictr_ws.o $(LIB_BSC) $(LIB_EXT)
	$(CC) $(@).o ltictr_api.o ltictr_child.o ltictr_xmlrpc.o ltictr_ws.o $(ELIB) $(SLIB) -O2 -o $@ 


$(LIB_BSC):
	(cd $(LIB_BSC_DIR) && make)

$(LIB_EXT):
	(cd $(LIB_EXT_DIR) && make)



install: ltictr_proxy 
	install -m 0755 ltictr_proxy /usr/local/bin
	chmod 0755 /var/ltictr_proxy


clean:
	rm -f *.o $(SUBJ) 


tgz:
	make clean
	(cd .. && tar zcf Dist/ltictr_proxy-${VERSION}.tgz ltictr_proxy)

