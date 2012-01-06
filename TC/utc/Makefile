ifeq ($(ARCH),target)
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=arm-linux-gcc -Wall
	CXX=arm-linux-g++ -Wall
else
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=gcc -Wall
    CXX=gcc -Wall
endif

TS1=utc_thumbnail_request_from_db_func
TS2=utc_thumbnail_get_data_func

LIBS = `pkg-config --libs libmedia-thumbnail`
LIBS +=-L/usr/lib/:/usr/lib/pkgconfig
LIBS +=$(TET_ROOT)/lib/tet3/tcm_s.o
LIBS +=-L$(TET_ROOT)/lib/tet3 -ltcm_s
LIBS +=-L$(TET_ROOT)/lib/tet3/ -lapi_s

INCS = -I. `pkg-config --cflags  libmedia-thumbnail`
INCS += -I$(TET_ROOT)/inc/tet3
INCS += -I/usr/include

CFLAGS = $(INCS) 
CC += $(CFLAGS)
LDFLAGS = $(LIBS)

all : $(TS10) $(TS11)

$(TS1): $(TS1).c
	$(CC) -o $(TS1) $(TS1).c $(LDFLAGS)

$(TS2): $(TS2).c
	$(CC) -o $(TS2) $(TS2).c $(LDFLAGS)
	

clean:
	rm -rf *~ *.o $(TS1) $(TS2)
	


