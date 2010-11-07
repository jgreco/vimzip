NAME = vimzip
CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -Wno-unused-parameter -pedantic -pipe
LIBS = -lutil -lpthread
OBJDIR = .build
OBJECTS = main.o
OBJECTS :=  $(addprefix ${OBJDIR}/,${OBJECTS})

$(NAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJECTS) -o $(NAME)


${OBJDIR}/%.o : %.c
	@if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi #create directory if it doesn't exist
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(NAME)
