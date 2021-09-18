AS = /opt/asem51/asem
EXEC = uart
SRC = uart-internal.a51

all: $(EXEC).hex

$(EXEC).hex: $(SRC)
	$(AS) $(SRC) $(EXEC).hex $(EXEC).lst

$(EXEC).bin: $(EXEC).hex
	objcopy -I ihex $(EXEC).hex -O binary $(EXEC).bin

clean:
	rm -f $(EXEC).bin $(EXEC).hex $(EXEC).lst

install: $(EXEC).hex
	minipro -p AT28C256 -f ihex -w $(EXEC).hex

program: $(EXEC).bin
	stty -F /dev/ttyUSB0 57600 cs8 -cstopb -parenb -ixon -crtscts
	echo -n 'QP' >/dev/ttyUSB0
	sx $(EXEC).bin >/dev/ttyUSB0 </dev/ttyUSB0
	echo -n 'BB' >/dev/ttyUSB0