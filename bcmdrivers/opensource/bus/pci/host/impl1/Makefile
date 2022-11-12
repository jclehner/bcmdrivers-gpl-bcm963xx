#
# Makefile for the bcm963xx PCIe host controller drivers
#  for ARM and MIPS based platforms with 4.1 kernel onwards
#
#

KDIR ?= /lib/modules/$(shell uname -r)/build

###################################################################
BRCM_BOARD ?= bcm963xx
ROOT = ../../../../../..
BRCMDRIVERS_DIR = $(ROOT)/bcmdrivers
BRCMSHARED_DIR = $(ROOT)/shared
PCIE_HCD_PATH = .
INC_BRCMDRIVER_PUB_PATH = $(BRCMDRIVERS_DIR)/opensource/include
INC_BRCMSHARED_PUB_PATH = $(BRCMSHARED_DIR)/opensource
INC_BRCMDRIVER_PRIV_PATH = $(INC_BRCMSHARED_PUB_PATH)/include
INC_BRCMBOARDPARMS_PATH = $(INC_BRCMDRIVER_PUB_PATH)/boardparams
PCIE_HCD_PATH=$(BRCMDRIVERS_DIR)/opensource/bus/pci/host/impl1
###################################################################

ccflags-y += -I$(PCIE_HCD_PATH)/common
ccflags-y += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)
ccflags-y += -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD)
ccflags-y += -I$(INC_BRCMSHARED_PUB_PATH)/pmc

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
	$(MAKE) -C $(KDIR) M=$$PWD modules

modules_install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	rm -f core *.o *.a *.s .*.o .*.cmd *.ko $(MODULE_NAME)* modules.*
	rm -f arm/*.o arm/.*.cmd
	rm -f common/*.o common/.*.cmd
	rm -f mips/*.o mips/.*.cmd
