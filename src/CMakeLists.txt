

add_library(xpllock SHARED
  lock.c
)


set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_CFLAGS} ${PTHREAD_CFLAGS} ${OPENSSL_CFLAGS}")


target_link_libraries(xpllock
)

install(TARGETS xpllock DESTINATION /usr/lib)
