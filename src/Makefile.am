bin_PROGRAMS	= magick

VERSION_MAJOR := $(shell echo $(VERSION) | cut -f1 -d'.')
VERSION_MINOR := $(shell echo $(VERSION) | cut -f2 -d'.')
VERSION_PATCH := $(shell echo $(VERSION) | cut -f3 -d'.')
VERSION_COMPILED := $(shell expr $(VERSION_MAJOR) + $(VERSION_MINOR)):$(VERSION_PATCH):$(VERSION_MINOR)

LIBS = @LIBINTL@ @LIBS@

magick_LDFLAGS	= -rdynamic
magick_SOURCES	= main.cpp magick.cpp config_init.cpp config_parse.cpp \
				  protocol.cpp storage.cpp server.cpp message.cpp service.cpp \
				  dependency.cpp storageinterface.cpp liveuser.cpp \
				  livechannel.cpp storeduser.cpp storednick.cpp storedchannel.cpp \
				  storedchannel2.cpp committee.cpp otherdata.cpp nickserv.cpp \
				  commserv.cpp chanserv.cpp

