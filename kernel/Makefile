obj-m += ai_oom_guard.o
obj-m += memory_monitor.o
obj-m += gpu_watcher.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean