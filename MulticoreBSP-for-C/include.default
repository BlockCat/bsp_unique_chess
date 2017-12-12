
#Optimisation flags:
OPT=${NOWARN} -O3 -funroll-loops -g -DNDEBUG
#OPT=-O0 -g -DMCBSP_SHOW_PINNING

#GNU Compiler Collection:
NOWARN=
CFLAGS=-Wextra ${OPT} -I.
CC=gcc -ansi -std=c99
CPPFLAGS=-Wextra ${OPT} -I.
CPP=g++ -ansi -std=c++98
LFLAGS=`${RELPATH}deplibs.sh ${CC}`
AR=ar

#clang LLVM compiler:
#NOWARN=-Wno-padded -Wno-missing-noreturn -Wno-cast-align -Wno-covered-switch-default -Wno-unreachable-code -Wno-format-nonliteral -Wno-float-equal
#CFLAGS=-Weverything ${OPT} -I.
#CC=clang -std=c99
#CPPFLAGS=-Weverything ${OPT} -I.
#CPP=clang++ -std=c++98
#LFLAGS=`${RELPATH}deplibs.sh ${CC}`
#AR=ar

#Intel C++ Compiler:
#NOWARN=-diag-disable 378
#CFLAGS=-Wall ${OPT} -I.
#CC=icc -std=c99
#CPPFLAGS=-ansi -Wall ${OPT} -I.
#CPP=icc
#LFLAGS=`${RELPATH}deplibs.sh ${CC}`
#AR=ar

#MinGW 32-bit Windows cross-compiler:
#NOWARN=
#PTHREADS_WIN32_PATH=pthreads-w32/
#CFLAGS=-Wextra ${OPT} -I. -I${PTHREADS_WIN32_PATH}include
#CC=i686-w64-mingw32-gcc -ansi -std=c99
#CPPFLAGS=-Wextra ${OPT} -I. -I${PTHREADS_WIN32_PATH}include
#CPP=i686-w64-mingw32-g++ -ansi -std=c++98
#LFLAGS=${RELPATH}${PTHREADS_WIN32_PATH}lib/x86/libpthreadGC2.a
#AR=i686-w64-mingw32-ar

#MinGW 64-bit Windows cross-compiler:
#NOWARN=
#PTHREADS_WIN32_PATH=pthreads-w32/
#CFLAGS=-Wextra ${OPT} -I. -I${PTHREADS_WIN32_PATH}include
#CC=x86_64-w64-mingw32-gcc -ansi -std=c99
#CPPFLAGS=-Wextra ${OPT} -I. -I${PTHREADS_WIN32_PATH}include
#CPP=x86_64-w64-mingw32-g++ -ansi -std=c++98
#LFLAGS=${RELPATH}${PTHREADS_WIN32_PATH}lib/x64/libpthreadGC2.a
#AR=x86_64-w64-mingw32-ar


VERSION=1.2.0
OBJECTS=mcutil.o mcinternal.o mcbsp.o
TESTOBJ=tests/internal.o tests/util.o tests/bsp.o tests/abort.o tests/spmd.o
RELPATH=./

%.o: %.c %.h
	${CC} ${CFLAGS} -c -o $@ $(^:%.h=)

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $^

%.opp: %.cpp %.hpp
	${CPP} ${CPPFLAGS} -c -o $@ $(^:%.hpp=)

%.opp: %.cpp
	${CPP} ${CPPFLAGS} -c -o $@ $^

