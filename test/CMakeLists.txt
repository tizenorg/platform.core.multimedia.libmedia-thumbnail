SET(TEST-THUMB "test-thumb.c")

INCLUDE(FindPkgConfig)
	pkg_check_modules(pkgs REQUIRED glib-2.0 gthread-2.0 dlog mm-fileinfo aul libexif ecore-evas evas mmutil-imgp mmutil-jpeg libmedia-utils vconf libtzplatform-config)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Wall -fPIE")

ADD_EXECUTABLE(test-thumb ${TEST-THUMB})
TARGET_LINK_LIBRARIES(test-thumb ${pkgs_LDFLAGS} ${MEDIATHUMB-LIB} )
