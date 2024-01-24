clang++ -std=c++11 -fPIC -I/tmp/245/pipy/include -I/usr/include/mysql -lpthread -shared /tmp/245/pipy/samples/nmi/mysql-nmi//mysql-nmi.cpp libmysqlclient.a libssl.a libcrypto.a -o mysql-nmi.so
