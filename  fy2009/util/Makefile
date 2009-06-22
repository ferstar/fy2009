# ====================================================================
# See the file LICENSE for redistribution information.
#
# Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
#
# Author: DreamFreelancer, zhangxb66@2008.sina.com
#
# [History]
# initialize: 2009-4-29
#====================================================================
include ../Make.defines

INC_FLAG=-I ../h
PATH_OUT=../lib

PATH_OBJ=./objs

OBJS=$(PATH_OBJ)/fy_base.o \
     $(PATH_OBJ)/fy_stream.o \
     $(PATH_OBJ)/fy_basex.o \
     $(PATH_OBJ)/fy_trace.o \
     $(PATH_OBJ)/fy_msg.o \
     $(PATH_OBJ)/fy_aio.o 

all:chk_lib_path chk_obj_path $(PATH_OUT)/fy_util.a

chk_lib_path:
	@if test ! -d $(PATH_OUT) ; \
	then \
		mkdir $(PATH_OUT); \
	fi

chk_obj_path:
	@if test ! -d $(PATH_OBJ); \
	then \
		mkdir $(PATH_OBJ); \
	fi

clean:
	rm -f $(PATH_OUT)/fy_util.a $(OBJS)

$(PATH_OUT)/fy_util.a:$(OBJS)
	ar rsuv $@ $(OBJS)

$(PATH_OBJ)/fy_base.o:fy_base.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_base.cpp
$(PATH_OBJ)/fy_stream.o:fy_stream.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_stream.cpp
$(PATH_OBJ)/fy_basex.o:fy_basex.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_basex.cpp
$(PATH_OBJ)/fy_trace.o:fy_trace.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_trace.cpp
$(PATH_OBJ)/fy_msg.o:fy_msg.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_msg.cpp
$(PATH_OBJ)/fy_aio.o:fy_aio.cpp
	$(CXX) $(DEFAULT_FLAGS) -c $(INC_FLAG) -o $@ fy_aio.cpp

