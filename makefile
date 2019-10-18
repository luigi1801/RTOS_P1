#
#  USAGE:
#     make          ... to build the program
#

include make.def

EXES= Project1.$(EXE) 

all: $(EXES)

Project1.$(EXE): Project1.$(OBJ)
	$(CLINKER) -o Project1$(EXE) Project1.$(OBJ) $(LIBS) $(OPTFLAGS)

clean:
	$(RM) Project1

.SUFFIXES: .c  .$(OBJ)

.c.$(OBJ):
	$(CC) $(CFLAGS) -c $<