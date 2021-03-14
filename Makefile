
CXX = g++

all : libElfLoader.a

.PHONY : all clean

libElfLoader.a : ElfLoader.o  ExeLoader.o
	$(AR) -r $@ $^

%.o : %.cc
	$(CXX) -O3 -std=c++11 -I. -c -o $@ $<

clean :
	rm *.o *.a

