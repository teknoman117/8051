CC = /opt/sdcc-4.1.6/bin/sdcc
EXEC = testfs.ihx
SRCC = testfs.c pff.c diskio.c
OBJ = $(SRCC:.c=.rel)
CFLAGS = -mmcs51 --model-small --iram-size 0x80
LDFLAGS = -mmcs51 --model-small --iram-size 0x80 --xram-loc 0x0000 --xram-size 0x8000 --code-loc 0x0000

all: $(EXEC)

install: $(EXEC)
	minipro -p AT28C256 -f ihex -w $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

$(EXEC).bin: $(EXEC)
	objcopy -I ihex $(EXEC) -O binary $(EXEC).bin

program: $(EXEC).bin
	stty -F /dev/ttyUSB0 57600 cs8 -cstopb -parenb -ixon -crtscts
	echo -n 'QP' >/dev/ttyUSB0
	sx $(EXEC).bin >/dev/ttyUSB0 </dev/ttyUSB0
	echo -n 'BB' >/dev/ttyUSB0

%.rel: %.c
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f $(EXEC) $(EXEC).bin $(OBJ) *.asm *.sym *.map *.mem *.lk *.rst *.lst
