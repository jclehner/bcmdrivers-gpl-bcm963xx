#
# Makefile for the bcm963xx PCIe host controller drivers
#  for ARM and MIPS based platforms with 4.1 kernel onwards
#
#

KDIR ?= /lib/modules/$(shell uname -r)/build

###################################################################
BRCM_BOARD = bcm963xx
ROOT = $(abspath $(PWD)/../../../../../../..)
###################################################################

ccflags-y += -DCONFIG_BCM94908=y
ccflags-y += -I$(M) -I$(M)/common
ccflags-y += -I$(ROOT)/bcmdrivers/opensource/include/bcm963xx
ccflags-y += -I$(ROOT)/shared/opensource/include/bcm963xx
ccflags-y += -I$(ROOT)/shared/opensource/include/pmc

#
# ARM platforms
#
ifeq ($(filter y,$(CONFIG_ARM) $(CONFIG_ARM64)),)
else
ccflags-y += -I$(PCIE_HCD_PATH)/arm
obj-m += arm/pcie-bcm963xx.o
ifeq ("$(CONFIG_ARM64)" ,"y")
#temporary until fill DT is implemented for arm64 platforms
#MODULE_OBJS += ../../../../../../kernel/$(notdir $(KERNEL_DIR))/drivers/pci/setup-irq.o
endif
endif

#
# MIPS platforms
#

ifeq ("$(CONFIG_MIPS)" ,"y")
ifdef BCM_KF # defined(CONFIG_BCM_KF_PCI_FIXUP)
ifdef CONFIG_MIPS_BCM963XX
ccflags-y += -I$(PCIE_HCD_PATH)/mips
obj-m += mips/pci-bcm963xx.o mips/fixup-bcm963xx.o mips/ops-bcm963xx.o
endif
endif # BCM_KF
endif

obj-m += common/pcie_common.o

modules:
	echo $(ROOT)
	$(MAKE) -C $(KDIR) M=$$PWD modules

modules_install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	rm -f core *.o *.a *.s .*.o .*.cmd *.ko $(MODULE_NAME)* modules.*
	rm -f arm/*.o arm/.*.cmd
	rm -f common/*.o common/.*.cmd
	rm -f mips/*.o mips/.*.cmd
