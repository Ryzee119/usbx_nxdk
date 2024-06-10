XBE_TITLE = usbx
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(CURDIR)/main.c

USBX_DIR = $(CURDIR)/lib/usb/usbx

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
	$(wildcard $(USBX_DIR)/common/usbx_host_controllers/src/ux_hcd_ohci_*.c) \
	$(filter-out 	$(USBX_DIR)/common/core/src/ux_utility_physical_address.c \
					$(USBX_DIR)/common/core/src/ux_utility_virtual_address.c, \
					$(wildcard $(USBX_DIR)/common/core/src/ux_utility_*.c)) \
	threadx_glue/glue.c \
	lib/nxdk/usb.c

USBX_FLAGS = \
	-I$(CURDIR)/lib \
	-I$(CURDIR)/lib/usb \
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
	-DUX_ENABLE_MEMORY_STATISTICS \
	-DUX_HOST_CLASS_STORAGE_NO_FILEX

THREADX_FLAGS = \
	-DTX_INCLUDE_USER_DEFINE_FILE \

WINUSB_DIR = $(CURDIR)/lib/winapi
WINUSB_SRCS = $(WINUSB_DIR)/winusb.c
WINUSB_CFLAGS = -I$(WINUSB_DIR)

CFLAGS += \
	-Og \
	-Wno-builtin-macro-redefined \
	-Wno-implicit-function-declaration \
	$(USBX_FLAGS) \
	$(THREADX_FLAGS) \
	$(WINUSB_CFLAGS) \
	-Wall -Wextra -Wpedantic \
	-Wformat=2 -Wno-unused-parameter -Wshadow \
	-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
	-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
	-Wno-keyword-macro



SRCS += \
	main.c \
	error_strings.c \
	$(USBX_SRCS) \
	$(WINUSB_SRCS)

NXDK_DIR ?= $(CURDIR)/../..

include $(NXDK_DIR)/Makefile
