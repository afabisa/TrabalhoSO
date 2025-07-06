# Define the C compiler to use
CC = gcc

# Define the compiler flags
# -Wall: Enable all common warnings
# -pthread: Link with the POSIX threads library
# -g: Include debugging information
CFLAGS = -Wall -pthread -g

# Define the executable name
TARGET = trabalho_final_so_gemini

# Define the source file
SRCS = trabalho_final_so_gemini.c

# Default target: builds the executable
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS)

# Rule to clean up compiled files and the executable
clean:
	rm -f $(TARGET) *.o
