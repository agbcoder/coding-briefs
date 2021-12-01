IDIR =-I. -I${REPO_DIR}/3rd-party/GTest/1.8.1.4/linux/include
LDIR =-L. -L${REPO_DIR}/3rd-party/GTest/1.8.1.4/linux/lib
CXX=g++
CXXFLAGS=-g -O0 -mavx2 $(IDIR)
CC=gcc
CFLAGS=-g -O0 -mavx2 $(IDIR)
ASM=nasm
ASMFLAGS=-f elf64 -g -F dwarf
LDFLAGS=$(LDIR)

ODIR=obj

#LIBS=-lm
LIBS=

#_DEPS = cincpp.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = bopt_avx2.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


#$(ODIR)/%.o: %.cpp $(DEPS)
$(ODIR)/%.o: %.cpp
	@if ! [ -e $(ODIR) ]; then mkdir -p $(ODIR); fi
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(IDIR)

# this is an afterthought
$(ODIR)/%.o: %.c
	@if ! [ -e $(ODIR) ]; then mkdir -p $(ODIR); fi
	$(CC) -c -o $@ $< $(CFLAGS) $(IDIR)

$(ODIR)/%.o: %.asm
	@if ! [ -e $(ODIR) ]; then mkdir -p $(ODIR); fi
	$(ASM) -o $@ $< $(ASMFLAGS)

all: mySimd tests

mySimd: $(ODIR)/main.o $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

tests: myTests

myTests: $(ODIR)/boptTests.o $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LIBS) -lgtest -lgtest_main -lpthread

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rmdir $(ODIR)
