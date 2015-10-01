IF("${ENABLE_DCM}" STREQUAL "true")
SET(THUMB-SERVER
        ../src/util/media-thumb-db.c
	    ../src/media-thumb-internal.c
	    thumb-server.c
	    thumb-server-internal.c
	    thumb-server-dcm.c)
ELSE("${ENABLE_DCM}" STREQUAL "true")
SET(THUMB-SERVER
        ../src/util/media-thumb-db.c
	    ../src/media-thumb-internal.c
	    thumb-server.c
	    thumb-server-internal.c)
ENDIF("${ENABLE_DCM}" STREQUAL "true")

INCLUDE(FindPkgConfig)
	pkg_check_modules(pkgs REQUIRED glib-2.0 gthread-2.0 dlog mm-fileinfo aul libexif ecore-evas evas mmutil-imgp mmutil-jpeg libmedia-utils vconf libtzplatform-config)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--hash-style=both -pie")

ADD_EXECUTABLE(media-thumbnail-server ${THUMB-SERVER})
TARGET_LINK_LIBRARIES(media-thumbnail-server ${pkgs_LDFLAGS} ${MEDIATHUMB-LIB} )
INSTALL(TARGETS media-thumbnail-server DESTINATION ${BINDIR})
