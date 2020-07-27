CC = gcc
CFLAGS =  -Wall -g -O
CXXFLAGS =
INCLUDE = -I./include

BIN = aectest

LIB1 = pjmedia

LIB = -lpjlib-util \
	  -lspeex  \
	  -lwebrtc  \
	  -lpj \
	  -lopus -lm -lpthread  \
	  -framework CoreAudio -framework CoreServices -framework AudioUnit \
	  -framework AudioToolbox -framework Foundation -framework AppKit \
	  -framework AVFoundation -framework CoreGraphics -framework QuartzCore  \
	  -framework CoreMedia  -L/usr/local/lib -L./lib -lSDL2   -framework Security

OBJ1 = alaw_ulaw.o alaw_ulaw_table.o clock_thread.o delaybuf.o echo_common.o \
		echo_port.o echo_speex.o echo_suppress.o echo_webrtc.o errno.o format.o port.c \
		silencedet.o types.o wsola.o

SRC1 = ./src/alaw_ulaw.c ./src/alaw_ulaw_table.c ./src/clock_thread.c ./src/delaybuf.c ./src/echo_common.c \
		./src/echo_port.c ./src/echo_speex.c ./src/echo_suppress.c ./src/echo_webrtc.c ./src/errno.c ./src/format.c ./src/port.c \
		./src/silencedet.c ./src/types.c ./src/wsola.c

OBJ2 = aectest.o 

SRC2 = ./src/aectest.c 

# $(OBJ1): $(SRC1)
# 	$(CC) $(CFLAGS) ${INCLUDE} -c $(SRC1)

# install: $(SRC2) $(SRC1)
# 	$(CC) $(CFLAGS) ${INCLUDE} -o $(BIN) $(SRC1) $(SRC2) $(LIB)

$(BIN): $(SRC2) $(SRC1)
	$(CC) $(CFLAGS) ${INCLUDE} -o $(BIN) $(SRC1) $(SRC2) $(LIB)

clean:
	rm -rf *.o 
	rm aectest