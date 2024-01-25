#clang++ -std=c++11 -fPIC -I/tmp/245/pipy/include -I/usr/include/mysql -lpthread -shared /tmp/245/pipy/samples/nmi/mysql-nmi//mysql-nmi.cpp libmysqlclient.a libssl.a libcrypto.a -o mysql-nmi.so

clang++ -std=c++11 -fPIC -I/root/mysql-nmi/pipy/include -I/root/mysql/mysql-8.0.33-linux-glibc2.17-x86_64-minimal/include/ -I/usr/include/mysql -lpthread -shared /root/mysql-nmi/pipy/samples/nmi/mysql-nmi//mysql-nmi.cpp libmysqlclient.a libssl.a libcrypto.a -o mysql-nmi.so



