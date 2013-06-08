
.PHONY:all

all:
	make -C example && make -C echo_server && make -C echo_client

clean:
	make clean -C example/ && make clean -C echo_server && make clean -C echo_client
