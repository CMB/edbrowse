#  Simple makefile to move to the src directory.
#  This only works if you are making the default target.

all :
	$(MAKE) -C src

clean :
	$(MAKE) -C src clean

