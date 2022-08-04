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
LIB1=libactspef_$(EXT).a
LIB2=spef_pass_$(EXT).so
LIB=$(LIB1) $(LIB2)
TARGETS=$(EXE)
TARGETLIBS=$(LIB)
TARGETINCS=spef.h spef.def
TARGETINCSUBDIR=act

LIBOBJ=spef.o

MAIN=main.o

OBJS=$(MAIN) $(LIBOBJ)
SHOBJS=spef_pass.os spef.os

SRCS=$(OBJS:.o=.cc) $(SHOBJS:.os=.cc)

include $(ACT_HOME)/scripts/Makefile.std

$(EXE): $(MAIN) $(LIB) $(LIBACTDEPEND)
	$(CXX) $(CFLAGS) $(MAIN) -o $(EXE) $(LIBACT) -lactspef

$(LIB1): $(LIBOBJ)
	ar ruv $(LIB1) $(LIBOBJ)
	$(RANLIB) $(LIB1)

$(LIB2): $(SHOBJS)
	$(ACT_HOME)/scripts/linkso $(LIB2) $(SHOBJS) $(SHLIBACTPASS)

doc:
	if [ ! -f Doxyfile ]; then doxygen -g; fi
	doxygen


-include Makefile.deps
