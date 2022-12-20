# Αρχεία .o
OBJS = main.o

# Ονομα αρχείου
FILE = file.txt

# Το εκτελέσιμο πρόγραμμα
EXEC = main

# Ορίσματα
ARGS = 2 2 $(FILE)

$(EXEC): $(OBJS)
	gcc $(OBJS) -o $(EXEC)

clean:
	rm -f $(OBJS) $(EXEC)

run: $(EXEC)
	./$(EXEC) $(ARGS)

