IDIR =.
CXX=g++
CXXFLAGS=-I$(IDIR)
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=obj
LDIR =.

#LIBS=-lm
LIBS=

#_DEPS = cincpp.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o md5.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


#$(ODIR)/%.o: %.cpp $(DEPS)
$(ODIR)/%.o: %.cpp
	@if ! [ -e $(ODIR) ]; then mkdir -p $(ODIR); fi
	$(CXX) -c -o $@ $< $(CXXFLAGS)

# this is an afterthought
$(ODIR)/%.o: %.c
	@if ! [ -e $(ODIR) ]; then mkdir -p $(ODIR); fi
	$(CC) -c -o $@ $< $(CFLAGS)

all: copyDir

copyDir: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rmdir $(ODIR)
