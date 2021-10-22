KVER ?= $(shell uname -r)

obj-m += intel-dimm.o

all:
	make -C /lib/modules/${KVER}/build M=$(PWD) modules

clean:
	make -C /lib/modules/${KVER}/build M=$(PWD) clean
