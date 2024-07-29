#-------------------------------------------------------------------------
#
#  Copyright (c) 2018 Rajit Manohar
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA  02110-1301, USA.
#
#-------------------------------------------------------------------------
EXE=test_spef.$(EXT)
EXE2=test_sdf.$(EXT)
LIB1=libactannotate_$(EXT).a
LIB2=annotate_pass_$(EXT).so
LIB3=libactannotate_sh_$(EXT).so
LIB=$(LIB1) $(LIB2)
TARGETS=$(EXE) $(EXE2)
TARGETLIBS=$(LIB)
TARGETINCS=spef.h spef.def sdf.h sdf.def
TARGETINCSUBDIR=act

LIBOBJ=spef.o sdf.o

MAIN=main.o
MAIN2=main2.o

OBJS=$(MAIN) $(LIBOBJ) $(MAIN2)
SHOBJS3=spef.os sdf.os
SHOBJS=annotate_pass.os $(SHOBJS3)

SRCS=$(OBJS:.o=.cc) $(SHOBJS:.os=.cc)

include $(ACT_HOME)/scripts/Makefile.std

$(EXE): $(MAIN) $(LIB) $(LIBACTDEPEND)
	$(CXX) $(CFLAGS) $(MAIN) -o $(EXE) $(LIBACT) -lactannotate

$(EXE2): $(MAIN2) $(LIB) $(LIBACTDEPEND)
	$(CXX) $(CFLAGS) $(MAIN2) -o $(EXE2) $(LIBACT) -lactannotate

$(LIB1): $(LIBOBJ)
	ar ruv $(LIB1) $(LIBOBJ)
	$(RANLIB) $(LIB1)

$(LIB2): $(SHOBJS)
	$(ACT_HOME)/scripts/linkso $(LIB2) $(SHOBJS) $(SHLIBACTPASS)

$(LIB3): $(SHOBJS3)
	$(ACT_HOME)/scripts/linkso $(LIB3) $(SHOBJS3) $(SHLIBACTPASS)


doc:
	if [ ! -f Doxyfile ]; then doxygen -g; fi
	doxygen


-include Makefile.deps
