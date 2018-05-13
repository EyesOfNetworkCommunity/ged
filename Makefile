# **********************************************************************************************
# Copyright (C) 2007 Jérémie Bernard, Michaël Aubertin
#
# This package is free software; you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this software;
# if not, write to :
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
# **********************************************************************************************

########################
# constant definitions #
########################

# installation target directory
prefix	       = /usr

# target name and modules
LIBGED	      := libged-1.5.so
LIBGEDQ	      := libgedq-1.5.so
LIBSGED	      := libged-1.5.a
LIBSGEDQ      := libgedq-1.5.a
GED	      := ged
GEDQ	      := gedq
MODDUMMY      := geddummy-1.5.so
MODMYSQL      := gedmysql-1.5.so
#MODHDB	      := gedhdb-1.5.so
#MODTDB	      := gedtdb-1.5.so
#MOD	      := $(MODDUMMY) $(MODHDB) $(MODMYSQL)
MOD	      := $(MODDUMMY) $(MODMYSQL)

# debug preprocessors
GEDDEBUG      := -D__GED_DEBUG_SQL__ -D__GED_DEBUG_SEM__

# preprocessor directives -D
GEDDEF        := -D__GED__ -D__GED_NTLM__ -D__GED_TUN__ $(GEDDEBUG) -fPIC
GEDQDEF	      := -D__GEDQ__ -D__GED_NTLM__ -D__GED_TUN__ $(GEDDEBUG) -fPIC

# library dependencies
LIBS	      := `pkg-config --libs libgenerics-1.3`
LIBS	      += `pkg-config --libs glib-2.0`
LIBS	      += -lssl -lpthread -lgcrypt -lz
MYSQLLIBS     := /usr/lib64/mysql/libmysqlclient.so
#BDBLIBS	      := /usr/lib64/libdb.so

# paths
SRCCOR	      := ./src
INCLUDE	      := -I./inc 
INCLUDEDEPS   := `pkg-config --cflags libgenerics-1.3` 
INCLUDEDEPS   += `pkg-config --cflags glib-2.0`

# gcc directives
CC 	      := g++
GEDFLAGS      := -fPIC -Wno-multichar -O2 $(GEDDEF)  $(INCLUDE) $(INCLUDEDEPS)
GEDQFLAGS     := -fPIC -Wno-multichar -O2 $(GEDQDEF) $(INCLUDE) $(INCLUDEDEPS)

TARGETS       := $(LIBGED) $(LIBGEDQ) $(LIBSGED) $(LIBSGEDQ) $(GED) $(GEDQ) $(MOD) 

# main binary objects
COROBJS	       = csocket.o			\
		 csocketclient.o		\
		 csocketserver.o		\
		 csocketsslclient.o		\
		 csocketsslserver.o		\
		 cgedbackend.o			\
		 gedcommon.o			\
		 ged.o

GEDQOBJS       = csocket.o			\
                 csocketclient.o		\
                 csocketsslclient.o		\
		 csocketserver.o		\
		 csocketsslserver.o		\
		 gedq.o				\
                 gedqcommon.o

#################
# Makefile body #
#################

# main targets
all : $(TARGETS)

dummy: $(MODDUMMY)

mysql: $(MODMYSQL)

#bdb : $(MODHDB) $(MODTDB)

# generic rules
csocket.o : $(SRCCOR)/csocket.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

csocketserver.o : $(SRCCOR)/csocketserver.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

csocketclient.o : $(SRCCOR)/csocketclient.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

csocketsslserver.o : $(SRCCOR)/csocketsslserver.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

csocketsslclient.o : $(SRCCOR)/csocketsslclient.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

cgedbackend.o : $(SRCCOR)/cgedbackend.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

gedcommon.o : $(SRCCOR)/gedcommon.c
	@echo "[CC] $< [ged side]"
	@$(CC) $(GEDFLAGS) -c $<

ged.o : $(SRCCOR)/ged.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

gedmain.o : $(SRCCOR)/gedmain.c
	@echo "[CC] $<"
	@$(CC) $(GEDFLAGS) -c $<

gedqmain.o : $(SRCCOR)/gedqmain.c
	@echo "[CC] $<"
	@$(CC) $(GEDQFLAGS) -c $<

gedqcommon.o : $(SRCCOR)/gedcommon.c
	@echo "[CC] $< [gedq side]"
	@$(CC) $(GEDQFLAGS) -o $@ -c $<

gedq.o : $(SRCCOR)/gedq.c
	@echo "[CC] $<"
	@$(CC) $(GEDQFLAGS) -c $<

#cgedbackendbdb.o : $(SRCCOR)/cgedbackendbdb.c
#	@echo "[CC] $<"
#	@$(CC) $(GEDFLAGS) -o $@ -c $<

#cgedbackendhdb.o : $(SRCCOR)/cgedbackendhdb.c
#	@echo "[CC] $<"
#	@$(CC) $(GEDFLAGS) -o $@ -c $<
#
#cgedbackendtdb.o : $(SRCCOR)/cgedbackendtdb.c
#	@echo "[CC] $<"
#	@$(CC) $(GEDFLAGS) -o $@ -c $<

# main targets
$(GED) : $(LIBGED) gedmain.o
	@echo "[LK] $@"
	@$(CC) -rdynamic $(GEDFLAGS) $^ -o $@ $(LIBS) $(LIBGED)

$(GEDQ) : $(LIBGEDQ) gedqmain.o
	@echo "[LK] $@"
	@$(CC) $(GEDQFLAGS) $^ -o $@ $(LIBS) $(LIBGEDQ)

$(LIBGED) : $(COROBJS)
	@echo "[LK] $@"
	@$(CC) -shared $(GEDFLAGS) $^ -o $@ $(LIBS)

$(LIBGEDQ) : $(GEDQOBJS) 
	@echo "[LK] $@"
	@$(CC) -shared $(GEDQFLAGS) $^ -o $@ $(LIBS)

$(LIBSGED) : $(COROBJS)
	@echo "[LK] $@"
	@ar cr  $(LIBSGED) $^
	@ranlib $(LIBSGED)

$(LIBSGEDQ) : $(GEDQOBJS)
	@echo "[LK] $@"
	@ar cr  $(LIBSGEDQ) $^
	@ranlib $(LIBSGEDQ)

gedmysql-1.5.so : $(SRCCOR)/cgedbackendmysql.c
	@echo "[LK] $@"
	@$(CC) -shared $(GEDFLAGS) $^ -o $@ $(MYSQLLIBS)

geddummy-1.5.so : $(SRCCOR)/cgedbackenddummy.c
	@echo "[LK] $@"
	@$(CC) -shared $(GEDFLAGS) $^ -o $@

#gedhdb-1.5.so : cgedbackendbdb.o cgedbackendhdb.o
#	@echo "[LK] $@"
#	@$(CC) -shared $(GEDFLAGS) $^ -o $@ $(BDBLIBS)

#gedtdb-1.5.so : cgedbackendbdb.o cgedbackendtdb.o
#	@echo "[LK] $@"
#	@$(CC) -shared $(GEDFLAGS) $^ -o $@ $(BDBLIBS)

clean :
	@echo -n "cleaning... "
	@rm -rf $(TARGETS) *.o *.dat
	@echo done.

