CFLAGS = -O2 -I./include -I./include/Mqtt

TARGET = GatewayServer_App

DIR_APPLICATION = Application
DIR_LIB = lib
DIR_LIBMQTT = Mqtt

DIR_OBJ = obj

DIRS = $(DIR_APPLICATION)		\
		$(DIR_LIB)				\
		$(DIR_LIBMQTT)

FILES = $(foreach dir, $(DIRS),$(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c, $(DIR_OBJ)/%.o, $(notdir $(FILES)))

$(TARGET):$(OBJS)
	-$(CC) -o $(TARGET) $(OBJS) -lpthread

$(DIR_OBJ)/%.o : $(DIR_APPLICATION)/%.c
	-$(CC) $(CFLAGS) -c $< -o $@ -lpthread

$(DIR_OBJ)/%.o : $(DIR_LIB)/%.c
	-$(CC) $(CFLAGS) -c $< -o $@

$(DIR_OBJ)/%.o : $(DIR_LIBMQTT)/%.c
	-$(CC) $(CFLAGS) -c $< -o $@

clean:
	-$(RM) $(TARGET)
	-$(RM) $(OBJS)
