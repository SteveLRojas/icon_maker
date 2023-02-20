CFLAGS = -std=gnu99 -O2
LDFLAGS = -lm
TARGET = icon_maker

.PHONY: all
all: $(TARGET)


TARGET_OBJS = icon_maker.o lodepng.o

$(TARGET): $(TARGET_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(TARGET_OBJS) $(CFLAGS) $(LDFLAGS) -o $@


icon_maker.o: icon_maker.c
lodepng.o: lodepng.c

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) *.o
