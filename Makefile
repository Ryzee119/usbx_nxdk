XBE_TITLE = usbx
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(CURDIR)/main.c

USBX_DIR = $(CURDIR)/usbx

# $(wildcard $(USBX_DIR)/common/usbx_host_classes/src/*.c)
# -I$(USBX_DIR)/common/usbx_host_classes/inc
# -DUX_STANDALONE
# -DUX_ENABLE_DEBUG_LOG
# -DUX_ENABLE_EVENT_TRACE

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
	-DUX_ENABLE_MEMORY_STATISTICS \
	-DUX_HOST_CLASS_STORAGE_NO_FILEX

CFLAGS += \
	-Og \
	-DDEBUG_CONSOLE \
	-Wno-builtin-macro-redefined \
	-Wno-implicit-function-declaration \
	$(USBX_FLAGS)

SRCS += \
	main.c \
	$(USBX_SRCS)

NXDK_DIR ?= $(CURDIR)/../..

include $(NXDK_DIR)/Makefile
