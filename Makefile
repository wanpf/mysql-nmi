ROOT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PIPY_DIR = $(abspath ${ROOT_DIR}/../../..)

BIN_DIR = $(abspath ${PIPY_DIR}/bin)
INC_DIR = $(abspath ${PIPY_DIR}/include)

PRODUCT = ${BIN_DIR}/mysql-nmi.so

CXXFLAGS = -std=c++11 -fPIC
LDFLAGS = -lpthread libmysqlclient.a libssl.a libcrypto.a

OS = $(shell uname -s)

ifeq (${OS},Darwin)
  LDFLAGS += -Wl,-flat_namespace,-undefined,dynamic_lookup
endif

all: ${PRODUCT}

${PRODUCT}: ${ROOT_DIR}/mysql-nmi.cpp
	clang++ ${CXXFLAGS} -I${INC_DIR} -I/root/mysql/mysql-8.0.33-linux-glibc2.17-x86_64-minimal/include/ -I/usr/include/mysql ${LDFLAGS} -shared $< -o $@

clean:
	rm -f ${PRODUCT}

test:
	${BIN_DIR}/pipy main.js --threads=max
