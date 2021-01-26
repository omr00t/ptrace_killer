# Compile this as a module.
obj-m += ptrace_killer.o

modules: 
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean: 
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
