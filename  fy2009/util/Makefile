include ../Make.defines

INC_FLAG=-I ../h
PATH_OUT=../lib

PATH_OBJ=./objs

OBJS=$(PATH_OBJ)/fy_base.o 

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

