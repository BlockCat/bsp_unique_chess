CC=g++
CFLAGS=-I. -I./src -I./includes -o3 -std=c++17

THCCHESS=thc-chess-library/chesslib.o
MCBSP=MulticoreBSP-for-C/lib/$(wildcard mcbsp*.a)
all: parallel1 parallel2 parallel3 sequential sequential2

parallel%: $(THCCHESS) $(MCBSP) src/parallel%.o	
	$(CC) $(CFLAGS) $(word 3,$^) $(THCCHESS) -LMulticoreBSP-for-C/lib -lmcbsp1.2.0 -lm -pthread -lrt -o $@	

sequential: $(THCCHESS) src/sequential.o src/GameTree.o
	$(CC) $(CFLAGS) src/sequential.o src/GameTree.o $< -LMulticoreBSP-for-C/lib -lmcbsp1.2.0 -lm -pthread -lrt  -o $@

sequential2: $(THCCHESS) src/sequential.1.o src/GameTree.o
	$(CC) $(CFLAGS) src/sequential.1.o src/GameTree.o $< -LMulticoreBSP-for-C/lib -lmcbsp1.2.0 -lm -pthread -lrt  -o $@

src/%.o: src/%.cpp	
	$(CC) $(CFLAGS) -c $< -o $@

$(THCCHESS): 
	$(MAKE) -C thc-chess-library

$(MCBSP):
	$(MAKE) -C MulticoreBSP-for-C library

clean: clean-thc clean-own clean-bsp

clean-bsp:
	$(MAKE) -C MulticoreBSP-for-C clean
clean-thc:
	$(MAKE) -C thc-chess-library clean

clean-own:	
	rm -f sequential*
	rm -f src/*.o
	rm -f parallel*

install:
	cp thc-chess-library/src/*.h includes	
	cp MulticoreBSP-for-C/*.hpp includes
	cp MulticoreBSP-for-C/*.h includes