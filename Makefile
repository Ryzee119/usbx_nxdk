XBE_TITLE = usbx
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(CURDIR)/main.c

USBX_DIR = $(CURDIR)/usbx
NETX_DIR = $(CURDIR)/netxduo

USBX_SRCS = \
	$(wildcard $(USBX_DIR)/common/core/src/ux_host_stack_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_hub_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_cdc_acm_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_gser_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_hid_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_pima_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_printer_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_prolific_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_storage_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_swar_*.c) \
	$(wildcard $(USBX_DIR)/common/usbx_host_classes/src/ux_host_class_video_*.c) \
	$(wildcard $(USBX_DIR)/common/core/src/ux_system_*.c) \
	$(wildcard $(USBX_DIR)/common/core/src/ux_trace_*.c) \
	$(filter-out $(USBX_DIR)/common/core/src/ux_utility_physical_address.c $(USBX_DIR)/common/core/src/ux_utility_virtual_address.c, $(wildcard $(USBX_DIR)/common/core/src/ux_utility_*.c)) \
	$(wildcard $(USBX_DIR)/common/usbx_host_controllers/src/ux_hcd_ohci_*.c) \
	threadx_glue/glue.c

USBX_FLAGS = \
	-I$(USBX_DIR)/ports/generic/inc \
	-I$(USBX_DIR)/common/core/inc \
	-I$(USBX_DIR)/common/usbx_host_classes/inc \
	-I$(USBX_DIR)/common/usbx_host_controllers/inc \
	-I$(CURDIR)/usbx_glue \
	-I$(CURDIR)/threadx_glue \
	-DUX_HOST_SIDE_ONLY \
	-DUX_ENABLE_ERROR_CHECKING \
	-DUX_ENABLE_ASSERT \
	-DUX_INCLUDE_USER_DEFINE_FILE \
	-DTX_INCLUDE_USER_DEFINE_FILE \
	-DUX_ENABLE_MEMORY_STATISTICS \
	-DUX_HOST_CLASS_STORAGE_NO_FILEX

NETX_SRCS = \
	$(wildcard $(NETX_DIR)/addons/*/*.c) \
	$(wildcard $(NETX_DIR)/common/src/*.c) \
	$(wildcard $(NETX_DIR)/crypto_libraries/src/*.c) \
	$(wildcard $(NETX_DIR)/nx_secure/src/*.c) \
	$(wildcard $(NETX_DIR)/tsn/src/*.c) \

NETX_FLAGS = \
	-I$(NETX_DIR)/common/inc \
	-I$(NETX_DIR)/crypto_libraries/inc \
	-I$(NETX_DIR)/crypto_libraries/ports/win32/vs_2019/inc \
	-I$(NETX_DIR)/nx_secure/inc \
	-I$(NETX_DIR)/nx_secure/ports \
	-I$(NETX_DIR)/ports/win32/vs_2019/inc \
	-I$(NETX_DIR)/tsn/inc \
	-DTX_INCLUDE_USER_DEFINE_FILE

CFLAGS += \
	-Og \
	-DDEBUG_CONSOLE \
	-Wno-builtin-macro-redefined \
	-Wno-implicit-function-declaration \
	$(USBX_FLAGS) \
	$(NETX_FLAGS)

SRCS += \
	main.c \
	nx_nvnet_network_driver.c \
	$(USBX_SRCS) \
	$(NETX_SRCS)

NXDK_DIR ?= $(CURDIR)/../..

include $(NXDK_DIR)/Makefile
