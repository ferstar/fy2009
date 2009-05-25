include ../Make.defines

INC_FLAG=-I../h
PATH_LIB= ../lib
PATH_BIN=bin
PATH_OBJS=./objs

###################################################
all:base stm
 
clean:clean_base clean_stm
###################################################
#test base,2006-7-13
base:fyt_base.cpp
	$(CXX) $(DEFAULT_FLAGS) $(INC_FLAG) -DFY_ENABLE_ASSERT -lpthread -g -o $(PATH_BIN)/$@ fyt_base.cpp $(PATH_LIB)/fy_util.a

#test stream
stm:fyt_stream.cpp
	$(CXX) $(DEFAULT_FLAGS) $(INC_FLAG) -DFY_ENABLE_ASSERT -lpthread -g -o $(PATH_BIN)/$@ fyt_stream.cpp $(PATH_LIB)/fy_util.a
###################################################
###################################################
clean_base:
	(rm -f $(PATH_BIN)/base)
clean_stm:
	(rm -f $(PATH_BIN)/stm)
