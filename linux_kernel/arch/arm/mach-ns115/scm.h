#ifndef SCM_H___
#define	SCM_H___

#include <mach/board-ns115.h>
#define SCM_BASE			NS115_SCM_BASE
#if 0
#define SCM_EFUSE_CONFIG		((SCM_BASE) + 0x00)
#define SCM_DEBUG_SIG_SEL		((SCM_BASE) + 0x04)
#define SCM_SYSTEM_ARB_SET		((SCM_BASE) + 0x08)
#define SCM_USB_AHB_STRAP		((SCM_BASE) + 0x0c)
#define SCM_USB_OHCI_STATUS		((SCM_BASE) + 0x10)
#define SCM_USB_EHCI_SIDEBAND		((SCM_BASE) + 0x14)
#define SCM_USB_PHY_ANALOG_TUNE_SHARP	((SCM_BASE) + 0x18)
#define SCM_USB_PHY_ANALOG_TNUE_PORTO	((SCM_BASE) + 0x1c)
#define SCM_SATA_CONTROL_0		((SCM_BASE) + 0x2c)
#define SCM_SATA_CONTROL_1		((SCM_BASE) + 0x30)
#define SCM_CPU_STATUS			((SCM_BASE) + 0x34)
#define SCM_WFI_ENTRY_REG		((SCM_BASE) + 0X28)
#define SCM_CPU1_FLOW_REG		((SCM_BASE) + SCM_CPU1_FLOW_OFFSET)

#define SCM_CPU1_FLOW_OFFSET			(0x3c)
#define SCM_WFI_ENTRY_REG_OFFSET		(0X38)
#define SCM_EFUSE_DATA_OFFSET			(0x80)

#define CPU1_FLOW_BOOT				(0)
#define CPU1_FLOW_HOTPLUG			(1)
#define CPU1_FLOW_SUSPEND			(2)
#define CPU1_FLOW_RESUME			(3)


#define SCM_NS2416_SYS_STATE_OFFSET		(SCM_WFI_ENTRY_REG_OFFSET + 0x08)
#define SCM_WFIENT_RSENT_OFFSET			(SCM_WFI_ENTRY_REG_OFFSET + 0x0C)
#define SCM_WFIENT_CTX_OFFSET			(SCM_WFI_ENTRY_REG_OFFSET + 0x10)

#endif

#endif //SCM_H__