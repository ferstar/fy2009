include ../Make.defines

INC_FLAG=-I../h
PATH_LIB= ../lib
PATH_BIN=bin
PATH_OBJS=./objs

###################################################
all:base
 
clean:clean_base
###################################################
#test base,2006-7-13
base:fyt_base.cpp
	$(CXX) $(DEFAULT_FLAGS) $(INC_FLAG) -g -o $(PATH_BIN)/$@ fyt_base.cpp

###################################################
###################################################
clean_base:
	(rm -f $(PATH_BIN)/base)
