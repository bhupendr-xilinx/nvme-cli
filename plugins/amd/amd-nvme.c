// SPDX-License-Identifier: GPL-2.0-or-later
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h> 
#include <ctype.h> 
#include <sys/stat.h>
#include <sys/time.h>

#include "nvme.h"
#include "libnvme.h"
#include "plugin.h"
#include "linux/types.h"
#include "nvme-print.h"
#include <poll.h>

#define CREATE_CMD
#include "amd-nvme.h"

#define DEFAULT_MCTP_NETWORK 1
#define DEFAULT_MCTP_EID 10

#define MCTP_TYPE_NVME          0x04
#define MCTP_TYPE_MIC           0x80

#if !defined(AF_MCTP)
#define AF_MCTP 45
#endif
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef __u8                    mctp_eid_t;

struct mctp_addr {
        mctp_eid_t              s_addr;
};

struct sockaddr_mctp {
        unsigned short int      smctp_family;
        __u16                   __smctp_pad0;
        unsigned int            smctp_network;
        struct mctp_addr        smctp_addr;
        __u8                    smctp_type;
        __u8                    smctp_tag;
        __u8                    __smctp_pad1;
};

enum nvme_mi_command {
	READ_NVME_MI_DATA_STRUCTURE,
	NVM_SUBSYSTEM_HEALTH_STATUS_POLL,
	CONTROLLER_HEALTH_STATUS_POLL,
	CONFIGURATION_SET,
	CONFIGURATION_GET,
	VPD_READ,
	VPD_WRITE,
	RESET,
	SES_RECEIVE,
	SES_SEND,
	MGMT_EP_BUFFER_READ,
	MGMT_EP_BUFFER_WRITE,
	SHUTDOWN,
};

typedef struct ae_supp_list_hdr_s {
	uint8_t num_ae_supp_ds;
	uint8_t ae_supp_list_ver_num;
	uint16_t ae_supp_total_len;
	uint8_t ae_supp_list_hdr_len;
} __attribute__((packed)) ae_supp_list_hdr_t;

typedef struct ae_supp_ds_s {
	uint8_t ae_supp_len;
	/* AE Supported Info */
	uint16_t ae_supp_id: 8;
	uint16_t rsvd: 7;
	uint16_t ae_supp_enable: 1;
} __attribute__((packed)) ae_supp_ds_t;

typedef struct ae_occ_uniq_id_s {
	uint8_t ae_occ_id;
	uint32_t ae_occ_scope_id_info;
	/* AE Occurrence Scope Info (AESSI) */
	uint8_t ae_occ_scope: 4;
	uint8_t rsvd: 4;
} __attribute__((packed)) ae_occ_uniq_id_t;

typedef struct ae_occ_list_hdr_s {
	uint8_t num_ae_occ_ds;
	uint8_t ae_occ_list_ver_num;
	/* AE Occurrence List Length Info (AEOLLI)*/
	uint32_t ae_occ_list_total_len: 23;
	uint32_t ae_occ_list_overflow: 1;
	uint32_t ae_occ_list_hdr_len: 8;
	/* AEM Transmission Info (AEMTI) */
	uint8_t aem_retry_count: 3;
	uint8_t aem_generation_number: 5;
} __attribute__((packed)) ae_occ_list_hdr_t;

typedef struct ae_occ_hdr_s {
	uint8_t ae_occ_hdr_len;
	uint8_t ae_occ_spec_info_len;
	uint8_t ae_occ_ven_spec_info_len;
	ae_occ_uniq_id_t ae_occ_uniq_id;
} __attribute__((packed)) ae_occ_hdr_t;

typedef struct ae_enable_list_hdr_s {
	uint8_t num_ae_enable_ds;
	uint8_t ae_enable_list_ver_num;
	uint16_t ae_enable_total_len;
	uint8_t ae_enable_list_hdr_len;
} __attribute__((packed)) ae_enable_list_hdr_t;

typedef struct ae_enable_ds_s {
	uint8_t ae_enable_len;
	/* AE Enable Info (AEEI) */
	uint8_t ae_enable_id;
	uint8_t rsvd: 7;
	uint8_t ae_enable: 1;
} __attribute__((packed)) ae_enable_ds_t;

typedef struct nvme_aem_hdr_s {
    struct nvme_mi_mi_resp_hdr mi_hdr;
    ae_occ_list_hdr_t *occ_list_hdr;
    ae_occ_hdr_t *occ_hdr;
} __attribute__((packed)) nvme_aem_hdr_t;

/* Asynchronous event identifier */
typedef enum ae_id_e {
	AE_CONTROLLER_READY,
	AE_CONTROLLER_FATAL_STATUS,
	AE_SHUTDOWN_STATUS,
	AE_CONTROLLER_ENABLE,
	AE_NAMESPACE_ATTR_CHANGED,
	AE_FIRMWARE_ACTIVATED,
	AE_COMPOSITE_TEMPERATURE,
	AE_PERCENT_DRIVE_LIFE_USED,
	AE_AVAILABLE_SPARE,
	AE_SMART_WARNINGS,
	AE_TELEMETRY_CTRL_INIT_DATA_AVL,
	AE_PCIE_LINK_ACTIVE,
	AE_SANITIZE_FAILURE_MODE,
	/* Vendor defined async events */
	AE_INCOMPATIBLE_BACKEND_SSD = 0xC0,
	AE_BACKEND_SSD_ASYNC_EVENT,
	AE_BACKEND_SSD_IP_CONFIG_MISMATCH,
	AE_BACKEND_SSD_SURPRISE_REMOVAL,
	AE_BACKEND_SSD_SURPRISE_ADDITION,
	AE_BACKEND_SSD_CMD_TIME_OUT,
	AE_BACKEND_SSD_CMD_COMP_WITH_ERROR,
	AE_BACKEND_SSD_RESET_BY_INTERPOSER,
	AE_BACKEND_SSD_PANIC_RESET_ACTION_SUCCESS,
	AE_BACKEND_SSD_PANIC_RESET_ACTION_FAILURE,
	AE_BACKEND_SSD_POWER_CYCLE_REQUESTED,
	AE_BACKEND_SSD_FAILURE_DEC_BY_INTERPOSER,
	AE_BACKEND_SSD_INVALID_POWER_ON_IP_CONFIG,
	AE_EXTENT_MGMT_EXTENT_FILL_THRESHOLD_FOR_SSD,
	AE_EXTENT_MGMT_NO_FREE_EXTENTS_ON_SSD,
	AE_MIRR_NAMESPACES_RESYNC_STARTED,
	AE_MIRR_NAMESPACES_RESYNC_FINISHED,
	AE_MIRR_NAMESPACES_DEGRADED_SSD_FAILURE,
	AE_MIRR_NAMESPACES_RESILVER_STARTED,
	AE_MIRR_NAMESPACES_RESILVER_COMPLETED,
	AE_IO_CONTROLLER_DISABLED,
	AE_NAMESPACE_DISABLED,
	AE_INTERPOSER_INIT_NVM_SUBSYS_RESET,
} ae_id_t;

typedef enum ae_occ_scope_e {
	AE_OCC_SCOPE_NAMESPACE,
	AE_OCC_SCOPE_CONTROLLER,
	AE_OCC_SCOPE_NVM_SUBSYS,
	AE_OCC_SCOPE_MGMT_EP,
	AE_OCC_SCOPE_PORT,
	AE_OCC_SCOPE_EG
} ae_occ_scope_t;

/* Data Structure Type (DTYP) values for Read NVMe-MI Data Structure command */
enum nvme_mi_data_structure_type {
	NVME_MI_DS_NVM_SUBSYSTEM_INFO = 0x00,
	NVME_MI_DS_PORT_INFO = 0x01,
	NVME_MI_DS_CONTROLLER_LIST = 0x02,
	NVME_MI_DS_CONTROLLER_INFO = 0x03,
	NVME_MI_DS_OPTIONAL_COMMAND_LIST = 0x04,
	NVME_MI_DS_MGMT_EP_BUFFER_CMD_SUPPORT_LIST = 0x05,
};

/* NVM Subsystem Information Data Structure (Section 5.7.1) */
typedef struct {
	__u8 nump;        /* Number of Ports */
	__u8 mjr;         /* NVMe-MI Major Version Number */
	__u8 mnr;         /* NVMe-MI Minor Version Number */
	__u8 nnsc;        /* NVMe-MI NVM Subsystem Capabilities */
} __attribute__((packed)) nvm_subsystem_info_t;

/* Port Information Data Structure (Section 5.7.2) */
typedef struct {
	__u8 prttyp;      /* Port Type */
	__u8 prtcap;      /* Port Capabilities */
	__u16 mmtus;      /* Maximum MCTP Transmission Unit Size */
	__u32 mebs;       /* Management Endpoint Buffer Size */
	__u8 ptsp[24];    /* Port Type Specific data */
} __attribute__((packed)) port_info_t;

/* Controller Information Data Structure (Section 5.7.4) */
typedef struct {
	__u8 portid;      /* Port Identifier */
	__u8 reserved1[4];
	__u8 prii;        /* PCIe Routing ID Information */
	__u16 pri;        /* PCIe Routing ID */
	__u16 pcivid;     /* PCI Vendor ID */
	__u16 pcidid;     /* PCI Device ID */
	__u16 pcisvid;    /* PCI Subsystem Vendor ID */
	__u16 pcisdid;    /* PCI Subsystem Device ID */
	__u8 pciesn;      /* PCIe Segment Number */
	__u8 reserved2[15];
} __attribute__((packed)) controller_info_t;

/* Optionally Supported Command Data Structure (Section 5.7.5) */
typedef struct {
	__u8 ctyp;        /* Command Type */
	__u8 opc;         /* Opcode */
} __attribute__((packed)) opt_supported_cmd_t;

/* Optionally Supported Command List Data Structure */
typedef struct {
	__u16 numcmd;     /* Number of Commands */
	opt_supported_cmd_t commands[];  /* Variable length list */
} __attribute__((packed)) opt_supported_cmd_list_t;

/* Management Endpoint Buffer Supported Command Data Structure */
typedef struct {
	__u8 ctyp;        /* Command Type */
	__u8 opc;         /* Opcode */
} __attribute__((packed)) mgmt_ep_buffer_cmd_t;

/* Management Endpoint Buffer Command Support List Data Structure */
typedef struct {
	__u16 numcmd;     /* Number of Commands */
	mgmt_ep_buffer_cmd_t commands[];  /* Variable length list */
} __attribute__((packed)) mgmt_ep_buffer_cmd_list_t;

/**
 * enum nvme_mi_resp_status_e - NVME-MI response status field.
 */
typedef enum nvme_mi_resp_status_e {
	STATUS_SUCCESS,
	STATUS_MORE_PROCESSING_REQUIRED,
	STATUS_INTERNAL_ERROR,
	STATUS_INVALID_COMMAND_OPCODE,
	STATUS_INVALID_PARAMETER,
	STATUS_INVALID_COMMAND_SIZE,
	STATUS_INVALID_COMMAND_INPUT_DATA_SIZE,
	STATUS_ACCESS_DENIED,
	STATUS_VPD_UPDATES_EXCEEDED = 0x20,
	STATUS_PCIE_INACCESSIBLE,
	STATUS_OPCODE_NOT_SUPPORTED = 0xE0,
	STATUS_FEATURE_NOT_SUPPORTED,
	STATUS_NOT_IMPLEMENTED,
	STATUS_NOT_SUPPORTED,
	STATUS_RESET_IN_PROGRESS,
	STATUS_INTERPOSER_NOT_CONFIGURED,
} nvme_mi_resp_status_t;

struct ae_config {
    char *enable_aeid_list;
	char *disable_aeid_list;
	__u8 aerd;
	__u8 aemd;
	__u8 numaee;
	__u8 aeelver;
	__u8 aeetl;
	__u8 aeelhl;
	__u8 csi;
};

struct get_config {
                __u8 cid;
                __u8 pid;
                __u8 csi;
};

struct health_status_change {  
    bool rdy;          // Ready (bit 0)  
    bool cfs;          // Controller Fatal Status (bit 1)  
    bool shst;         // Shutdown Status (bit 2)  
    bool nssro;        // NVM Subsystem Reset Occurred (bit 3)  
    bool ceco;         // Controller Enable Change Occurred (bit 4)  
    bool nac;          // Namespace Attribute Changed (bit 5)  
    bool fa;           // Firmware Activated (bit 6)  
    bool cschng;       // Controller Status Change (bit 7)  
    bool ctemp;        // Composite Temperature (bit 8)  
    bool pdlu;         // Percentage Used (bit 9)  
    bool spare;        // Available Spare (bit 10)  
    bool cwarn;        // Critical Warning (bit 11)  
    bool tcida;        // Telemetry Controller-Initiated Data Available (bit 12)  
    __u32 reserved : 19;    // Reserved (bits 13 to 31)  
};

#if 0
struct composite_controller_status_flags {  
	__u16 rdy : 1;          // Ready (bit 0)  
	__u16 cfs : 1;          // Controller Fatal Status (bit 1)  
	__u16 shst : 1;         // Shutdown Status (bit 2)
	__u16 reserved_1 : 1;    // Reserved (bit 3 )  
	__u16 nssro : 1;        // NVM Subsystem Reset Occurred (bit 4)  
	__u16 ceco : 1;         // Controller Enable Change Occurred (bit 5)  
	__u16 nac : 1;          // Namespace Attribute Changed (bit 6)  
	__u16 fa : 1;           // Firmware Activated (bit 7)  
	__u16 cschng : 1;       // Controller Status Change (bit 8)  
	__u16 ctemp : 1;        // Composite Temperature (bit 9)  
	__u16 pdlu : 1;         // Percentage Used (bit 10)  
	__u16 spare : 1;        // Available Spare (bit 11)  
	__u16 cwarn : 1;        // Critical Warning (bit 12)  
	__u16 tcida : 1;        // Telemetry Controller-Initiated Data Available (bit 13)  
	__u16 reserved_2 : 2;    // Reserved (bits 14 to 15 )  
};
#endif

/**  
 * Controller Health Status Poll – NVMe Management Dword 0  
 */  


struct controller_health_status_poll_dwd0 {
   		__u32 starting_controller_id;  // Bits 0-15: Starting Controller ID (SCTCTL)
		__u32 maximum_response_entries; // Bits 16-23: Maximum Response Entries (MAXRENT)
   		__u32 include_pci_functions;      // Bit 24: Include PCI Functions (INCF)
   		__u32 include_sr_iov_physical_functions; // Bit 25: Include SR-IOV Physical Functions (INCPFI)
   		__u32 include_sr_iov_virtual_functions;  // Bit 26: Include SR-IOV Virtual Functions (INCVI)
   		__u32 reserved2;                  // Bits 27-30: Reserved
   		__u32 report_all;                 // Bit 31: Report All (ALL)
};

/**  
 * Controller Health Status Poll – NVMe Management Dword 1  
 */  
struct controller_health_status_poll_dwd1{  
   	__u32 controller_status_changes;       // Bit 0: Controller Status Changes (CSTS)  
   	__u32 composite_temperature_changes;   // Bit 1: Composite Temperature Changes (CTEMP)  
   	__u32 percentage_used;                 // Bit 2: Percentage Used (PDLU)  
   	__u32 available_spare;                 // Bit 3: Available Spare (SPARE)  
   	__u32 critical_warning;                // Bit 4: Critical Warning (CWARN)  
   	__u32 reserved;                       // Bits 5-30: Reserved  
   	__u32 clear_changed_flags;             // Bit 31: Clear Changed Flags (CCF)  
};


/**  
 * NVM Subsystem Health Data Structure (NSHDS)  
 * As defined in NVM Express Management Interface Specification, Revision 2.0, Figure 108  
 */  
typedef struct {  
    /* Byte 0: NVM Subsystem Status (NSS) */  
    union {  
        uint8_t raw;  
        struct {  
            uint8_t reserved1                    : 2;  /* Bits 0-1: Reserved */  
            uint8_t port1_pcie_link_active       : 1;  /* Bit 2: P1LA - Port 1 PCIe Link Active */  
            uint8_t port0_pcie_link_active       : 1;  /* Bit 3: P0LA - Port 0 PCIe Link Active */  
            uint8_t reset_not_required           : 1;  /* Bit 4: RNR - Reset Not Required */  
            uint8_t drive_functional             : 1;  /* Bit 5: DF - Drive Functional */  
            uint8_t sanitize_failure_mode        : 1;  /* Bit 6: SFM - Sanitize Failure Mode */  
            uint8_t aem_transmission_failure     : 1;  /* Bit 7: ATF - AEM Transmission Failure */  
        } bits;  
    } nvm_subsystem_status;  
      
    /* Byte 1: SMART Warnings (SW) */  
    /* Inverted value of the Critical Warning field of SMART / Health Information log page */  
    uint8_t smart_warnings;  
      
    /* Byte 2: Composite Temperature (CTEMP) */  
    /*   
     * Range interpretation:  
     * 00h to 7Eh: 0°C to 126°C (value is temperature in Celsius)  
     * 7Fh: ≥ 127°C  
     * 80h: Temperature data is greater than 5s old  
     * 81h: Temperature data not accurate due to sensor failure  
     * 82h to C3h: Reserved  
     * C4h: ≤ -60°C  
     * C5h to FFh: -59°C to -1°C (two's complement of temperature in Celsius)  
     */  
    uint8_t composite_temperature;  
      
    /* Byte 3: Percentage Drive Life Used (PDLU) */  
    /* 0-254: Percentage of drive life used, 255: ≥ 255% */  
    uint8_t percentage_drive_life_used;  
      
    union {
        uint8_t raw[2];
        struct {
            uint16_t ready : 1;                              // Bit 0: Ready (RDY)
            uint16_t controller_fatal_status : 1;            // Bit 1: Controller Fatal Status (CFS)
            uint16_t shutdown_status : 1;                    // Bit 2: Shutdown Status (SHST)
            uint16_t reserved2 : 1;                          // Bit 3: Reserved
            uint16_t nvm_subsystem_reset_occurred : 1;       // Bit 4: NVM Subsystem Reset Occurred (NSSRO)
            uint16_t controller_enable_change_occurred : 1;  // Bit 5: Controller Enable Change Occurred (CECO)
            uint16_t namespace_attribute_changed : 1;        // Bit 6: Namespace Attribute Changed (NAC)
            uint16_t firmware_activated : 1;                 // Bit 7: Firmware Activated (FA)
            uint16_t controller_status_change : 1;           // Bit 8: Controller Status Change (CSTS)
            uint16_t composite_temperature_change : 1;       // Bit 9: Composite Temperature Change (CTEMP)
            uint16_t percentage_used : 1;                    // Bit 10: Percentage Used (PDLU)
            uint16_t available_spare : 1;                    // Bit 11: Available Spare (SPARE)
            uint16_t critical_warning : 1;                   // Bit 12: Critical Warning (CWARN)
            uint16_t telemetry_controller_initiated_data_available : 1; // Bit 13: TCIDA
            uint16_t reserved1 : 2;                         // Bits 14-15: Reserved
        } bits;
    } flags;

    /* Bytes 4-5: Composite Controller Status (CCS) */  
    //struct composite_controller_status_flags ccs;  
      
    /* Bytes 6-7: Reserved */  
    uint8_t reserved2[2];  
      
} __attribute__((packed)) nvm_subsystem_health_data_structure_t;


int parse_mi_resp_status(uint8_t status, uint8_t *resp_buffer) {
	int rc = -1;
	switch (status) {
		case STATUS_SUCCESS:
			printf("Status: Success\n");
			break;
		case STATUS_MORE_PROCESSING_REQUIRED:
			printf("Status: More Processing Required\n");
			break;
		case STATUS_INTERNAL_ERROR:
			printf("Status: Internal Error\n");
			break;
		case STATUS_INVALID_COMMAND_OPCODE:
			printf("Status: Invalid Command Opcode\n");
			break;
		case STATUS_INVALID_PARAMETER:
			printf("Status: Invalid Parameter, parameter error locations are:\n");
			printf("Invalid Parameter Bit location(BITLOC): %02x\n", resp_buffer[0] & 0x07);
			printf("Invalid Parameter Byte location(BYTLOC): %04x\n", (resp_buffer[1] << 8) | resp_buffer[2]);
			break;
		case STATUS_INVALID_COMMAND_SIZE:
			printf("Status: Invalid Command Size\n");
			break;
		case STATUS_INVALID_COMMAND_INPUT_DATA_SIZE:
			printf("Status: Invalid Command Input Data Size\n");
			break;
		case STATUS_ACCESS_DENIED:
			printf("Status: Access Denied\n");
			break;
		case STATUS_VPD_UPDATES_EXCEEDED:
			printf("Status: VPD Updates Exceeded\n");
			break;
		case STATUS_PCIE_INACCESSIBLE:
			printf("Status: PCIe Inaccessible\n");
			break;
		case STATUS_OPCODE_NOT_SUPPORTED:
			printf("Status: Opcode Not Supported\n");
			break;
		case STATUS_FEATURE_NOT_SUPPORTED:
			printf("Status: Feature Not Supported\n");
			break;
		case STATUS_NOT_IMPLEMENTED:
			printf("Status: Not Implemented\n");
			break;
		case STATUS_NOT_SUPPORTED:
			printf("Status: Not Supported\n");
			rc = 1;
			break;
		case STATUS_RESET_IN_PROGRESS:
			printf("Status: Reset In Progress\n");
			break;
		case STATUS_INTERPOSER_NOT_CONFIGURED:
			printf("Status: Interposer Not Configured\n");
			break;
		default:
			printf("Status: Unknown\n");
			break;
	}
	return rc;
}

void parse_ae_occ_scope (uint32_t ae_occ_scope_id_info, uint8_t ae_occ_scope) {
	switch (ae_occ_scope) {
		case AE_OCC_SCOPE_NAMESPACE:
			printf("  AE Occurrence Scope: Namespace\n");
			printf("  AE Occurrence Namespace ID (AEONSID): %08x\n", ae_occ_scope_id_info);
			break;
		case AE_OCC_SCOPE_CONTROLLER:
			printf("  AE Occurrence Scope: Controller\n");
			printf("  AE Occurrence Controller ID (AEOCID): %04x\n", ae_occ_scope_id_info & 0xFFFF);
			break;
		case AE_OCC_SCOPE_NVM_SUBSYS:
			printf("  AE Occurrence Scope: NVM Subsystem\n");
			break;
		case AE_OCC_SCOPE_MGMT_EP:
			printf("  AE Occurrence Scope: Management Endpoint\n");
			printf("  AE Occurrence Management Endpoint ID (AEOMEID): %02x\n", ae_occ_scope_id_info & 0xFF);
			break;
		case AE_OCC_SCOPE_PORT:
			printf("  AE Occurrence Scope: Port\n");
			printf("  AE Occurrence Port ID (AEOPID): %02x\n", ae_occ_scope_id_info & 0xFFFF);
			printf("  AE Occurrence Port Type (AEOPT): %d\n", (ae_occ_scope_id_info >> 16) & 0x1);
			break;
		case AE_OCC_SCOPE_EG:
			printf("  AE Occurrence Scope: Event Group\n");
			printf("  AE Occurrence Endurance Group ID (AEOEGID): %04x\n", ae_occ_scope_id_info & 0xFFFF);
			break;
		default:
			printf("  AE Occurrence Scope: Unknown\n");
			break;
	}
}

void get_ae_occ_ven_spec_info(uint8_t *ae_occ_ven_spec_info) {
	printf("  AE Occurrence Specific Info: ");
	for (int i = 6; i >= 0; i--) {
    	printf("%02x", ae_occ_ven_spec_info[i]);
	}
	printf("\n");
}

void parse_ae_occ_ven_spec_info(uint8_t ae_occ_id, 
								uint32_t ae_occ_scope_id_info, 
								uint8_t ae_occ_scope, 
								uint8_t *ae_occ_ven_spec_info) {
	printf("Parsing AE Occurrence Vendor Specific Info:\n");
	parse_ae_occ_scope(ae_occ_scope_id_info, ae_occ_scope);
	printf("  AE Occurrence ID: %02x\n", ae_occ_id);
	switch (ae_occ_id) {
		case AE_CONTROLLER_READY:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_CONTROLLER_FATAL_STATUS:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_CONTROLLER_ENABLE:
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_INCOMPATIBLE_BACKEND_SSD:
			printf("  AE Occurrence ID: Incompatible Backend SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_ASYNC_EVENT:
			printf("  AE Occurrence ID: Backend SSD Async Event\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_IP_CONFIG_MISMATCH:
			printf("  AE Occurrence ID: Backend SSD IP Config Mismatch\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_SURPRISE_REMOVAL:
			printf("  AE Occurrence ID: Backend SSD Surprise Removal\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_SURPRISE_ADDITION:
			printf("  AE Occurrence ID: Backend SSD Surprise Addition\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_CMD_TIME_OUT:
			printf("  AE Occurrence ID: Backend SSD Command Timeout\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_CMD_COMP_WITH_ERROR:
			printf("  AE Occurrence ID: Backend SSD Command Completion with Error\n");
			break;
		case AE_BACKEND_SSD_RESET_BY_INTERPOSER:
			printf("  AE Occurrence ID: Backend SSD Reset by Interposer\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_PANIC_RESET_ACTION_SUCCESS:
			printf("  AE Occurrence ID: Backend SSD Panic Reset Action Success\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_PANIC_RESET_ACTION_FAILURE:
			printf("  AE Occurrence ID: Backend SSD Panic Reset Action Failure\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_POWER_CYCLE_REQUESTED:
			printf("  AE Occurrence ID: Backend SSD Power Cycle Requested\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_BACKEND_SSD_FAILURE_DEC_BY_INTERPOSER:
			printf("  AE Occurrence ID: Backend SSD Failure Declared by Interposer\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_EXTENT_MGMT_EXTENT_FILL_THRESHOLD_FOR_SSD:
			printf("  AE Occurrence ID: Extent Management Extent Fill Threshold for SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_EXTENT_MGMT_NO_FREE_EXTENTS_ON_SSD:
			printf("  AE Occurrence ID: Extent Management No Free Extents on SSD\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESYNC_STARTED:
			printf("  AE Occurrence ID: Mirror Namespaces Resync Started\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);	
			break;
		case AE_MIRR_NAMESPACES_RESYNC_FINISHED:
			printf("  AE Occurrence ID: Mirror Namespaces Resync Finished\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_DEGRADED_SSD_FAILURE:
			printf("  AE Occurrence ID: Mirror Namespaces Degraded SSD Failure\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESILVER_STARTED:
			printf("  AE Occurrence ID: Mirror Namespaces Resilver Started\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_MIRR_NAMESPACES_RESILVER_COMPLETED:
			printf("  AE Occurrence ID: Mirror Namespaces Resilver Completed\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_IO_CONTROLLER_DISABLED:
			printf("  AE Occurrence ID: IO Controller Disabled\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_NAMESPACE_DISABLED:
			printf("  AE Occurrence ID: Namespace Disabled\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		case AE_INTERPOSER_INIT_NVM_SUBSYS_RESET:
			printf("  AE Occurrence ID: Interposer Init NVM Subsystem Reset\n");
			get_ae_occ_ven_spec_info(ae_occ_ven_spec_info);
			break;
		default:
			printf("  AE Occurrence ID: Unknown\n");
			break;
	}
	
}

void parse_ae_occ_spec_info(uint8_t ae_occ_id, 
							uint32_t ae_occ_scope_id_info, 
							uint8_t ae_occ_scope, 
							uint8_t ae_occ_spec_info) {
	printf("Parsing AE Occurrence Specific Info:\n");
	parse_ae_occ_scope(ae_occ_scope_id_info, ae_occ_scope);
	printf("  AE Occurrence ID: %02x\n", ae_occ_id);
	switch (ae_occ_id) {	
		case AE_SHUTDOWN_STATUS:
			printf("  Shutdown Status Value (SSV): %d\n", ae_occ_spec_info & 0x3);
			printf("  Shutdown Type Value (STV): %d\n", (ae_occ_spec_info >> 2) & 0x1);
			break;
		case AE_NAMESPACE_ATTR_CHANGED:
			break;
		case AE_FIRMWARE_ACTIVATED:
			printf("  Firmware Slot: %d\n", ae_occ_spec_info & 0x3);
			printf("  Next Firmware Slot Activated at Controller Reset: %d\n", (ae_occ_spec_info >> 4) & 0x3);
			break;
		case AE_COMPOSITE_TEMPERATURE:
			printf("  Composite Temperature Value (CTV): %d\n", ae_occ_spec_info);
			break;
		case AE_PERCENT_DRIVE_LIFE_USED:
			printf("  Max Drive Life Used Change: %d\n", ae_occ_spec_info);
			break;
		case AE_AVAILABLE_SPARE:
			printf("  Min Available Spare (MAS): %d\n", ae_occ_spec_info);
			break;
		case AE_SMART_WARNINGS:
			printf("  SMART Warnings Value (SWV): %d\n", ae_occ_spec_info);
			break;
		case AE_TELEMETRY_CTRL_INIT_DATA_AVL:
			printf("  AE Occurrence ID: Telemetry Controller Init Data Available (ID: %02x)\n", ae_occ_id);
			break;
		case AE_PCIE_LINK_ACTIVE:
			printf("  AE Occurrence ID: PCIe Link Active (ID: %02x)\n", ae_occ_id);
			break;
		case AE_SANITIZE_FAILURE_MODE:
			printf("  AE Occurrence ID: Sanitize Failure Mode (ID: %02x)\n", ae_occ_id);
			break;
		default:
			printf("  AE Occurrence ID: Unknown (ID: %02x)\n", ae_occ_id);
			break;
	}
	
}

/**
 * Parse and display NVM Subsystem Information Data Structure
 * @param subsys_info: Pointer to the NVM Subsystem Information structure
 */
static void parse_nvm_subsystem_info(const nvm_subsystem_info_t *subsys_info)
{
	printf("=== NVM Subsystem Information Data Structure ===\n");
	printf("Number of Ports (NUMP): %u\n", subsys_info->nump);
	printf("NVMe-MI Major Version Number (MJR): %u\n", subsys_info->mjr);
	printf("NVMe-MI Minor Version Number (MNR): %u\n", subsys_info->mnr);
	
	printf("NVMe-MI NVM Subsystem Capabilities (NNSC): 0x%02x\n", subsys_info->nnsc);
	printf("  Status Reporting Enhancements (SRE): %s\n", 
		   (subsys_info->nnsc & 0x01) ? "Supported" : "Not Supported");
	printf("  Reserved bits [7:1]: 0x%02x\n", (subsys_info->nnsc >> 1) & 0x7F);
}

/**
 * Parse and display Port Information Data Structure  
 * @param port_info: Pointer to the Port Information structure
 */
static void parse_port_info(const port_info_t *port_info)
{
	const char *port_types[] = {"Inactive", "PCIe", "2-Wire", "Reserved"};
	
	printf("=== Port Information Data Structure ===\n");
	printf("Port Type (PRTTYP): %u (%s)\n", port_info->prttyp,
		   port_info->prttyp < 3 ? port_types[port_info->prttyp] : "Reserved");
		   
	printf("Port Capabilities (PRTCAP): 0x%02x\n", port_info->prtcap);
	printf("  Command Initiated Auto Pause Supported (CIAPS): %s\n",
		   (port_info->prtcap & 0x01) ? "Yes" : "No");
	printf("  Asynchronous Event Messages Supported (AEMS): %s\n",
		   (port_info->prtcap & 0x02) ? "Yes" : "No");
	
	printf("Maximum MCTP Transmission Unit Size (MMTUS): %u bytes\n", port_info->mmtus);
	printf("Management Endpoint Buffer Size (MEBS): %u bytes\n", port_info->mebs);
	
	if (port_info->prttyp == 1) { /* PCIe */
		printf("=== PCIe Port Specific Data ===\n");
		printf("PCIe Maximum Payload Size: %u\n", port_info->ptsp[0]);
		printf("PCIe Supported Link Speeds Vector: 0x%02x\n", port_info->ptsp[1]);
		printf("PCIe Current Link Speed: %u\n", port_info->ptsp[2]);
		printf("PCIe Maximum Link Width: %u\n", port_info->ptsp[3]);
		printf("PCIe Negotiated Link Width: %u\n", port_info->ptsp[4]);
		printf("PCIe Port Number: %u\n", port_info->ptsp[5]);
	} else if (port_info->prttyp == 2) { /* 2-Wire */
		printf("=== 2-Wire Port Specific Data ===\n");
		printf("Current VPD Address (CVPDADDR): 0x%02x\n", port_info->ptsp[0]);
		printf("Maximum VPD Access Frequency (MVPDFREQ): %u\n", port_info->ptsp[1]);
		printf("Current Management Endpoint Address (CMEADDR): 0x%02x\n", port_info->ptsp[2]);
		printf("2-Wire Protocols Supported (TWPRT): 0x%02x\n", port_info->ptsp[3]);
		printf("NVMe Basic Management (NVMEBM): 0x%02x\n", port_info->ptsp[4]);
	}
}

/**
 * Parse and display Controller Information Data Structure
 * @param ctrl_info: Pointer to the Controller Information structure  
 */
static void parse_controller_info(const controller_info_t *ctrl_info)
{
	printf("=== Controller Information Data Structure ===\n");
	printf("Port Identifier (PORTID): %u\n", ctrl_info->portid);
	printf("PCIe Routing ID Information (PRII): 0x%02x\n", ctrl_info->prii);
	printf("  PCIe Routing ID Valid (PCIERIV): %s\n", 
		   (ctrl_info->prii & 0x01) ? "Valid" : "Invalid");
		   
	printf("PCIe Routing ID (PRI): 0x%04x\n", ctrl_info->pri);
	printf("  PCI Bus Number (PCIBN): %u\n", (ctrl_info->pri >> 8) & 0xFF);
	printf("  PCI Device Number (PCIDN): %u\n", (ctrl_info->pri >> 3) & 0x1F);
	printf("  PCI Function Number (PCIFN): %u\n", ctrl_info->pri & 0x07);
	
	printf("PCI Vendor ID (PCIVID): 0x%04x\n", ctrl_info->pcivid);
	printf("PCI Device ID (PCIDID): 0x%04x\n", ctrl_info->pcidid);
	printf("PCI Subsystem Vendor ID (PCISVID): 0x%04x\n", ctrl_info->pcisvid);
	printf("PCI Subsystem Device ID (PCISDID): 0x%04x\n", ctrl_info->pcisdid);
	printf("PCIe Segment Number (PCIESN): %u\n", ctrl_info->pciesn);
}

/**
 * Parse and display Optionally Supported Command List Data Structure
 * @param cmd_list: Pointer to the command list structure
 * @param data_len: Length of the data structure
 */
static void parse_opt_supported_cmd_list(const opt_supported_cmd_list_t *cmd_list, __u16 data_len)
{
	printf("=== Optionally Supported Command List Data Structure ===\n");
	printf("Number of Commands (NUMCMD): %u\n", cmd_list->numcmd);
	
	if (cmd_list->numcmd > 0) {
		printf("Command List:\n");
		for (int i = 0; i < cmd_list->numcmd && (i * 2 + 2) < data_len; i++) {
			printf("  Command %d:\n", i);
			printf("    Command Type (CTYP): 0x%02x\n", cmd_list->commands[i].ctyp);
			printf("    NVMe-MI Message Type (NMIMT): 0x%02x\n", 
				   (cmd_list->commands[i].ctyp >> 3) & 0x0F);
			printf("    Opcode (OPC): 0x%02x\n", cmd_list->commands[i].opc);
		}
	}
}

/**
 * Parse and display Management Endpoint Buffer Command Support List Data Structure
 * @param cmd_list: Pointer to the command list structure
 * @param data_len: Length of the data structure
 */
static void parse_mgmt_ep_buffer_cmd_list(const mgmt_ep_buffer_cmd_list_t *cmd_list, __u16 data_len)
{
	printf("=== Management Endpoint Buffer Command Support List Data Structure ===\n");
	printf("Number of Commands (NUMCMD): %u\n", cmd_list->numcmd);
	
	if (cmd_list->numcmd > 0) {
		printf("Command List:\n");
		for (int i = 0; i < cmd_list->numcmd && (i * 2 + 2) < data_len; i++) {
			printf("  Command %d:\n", i);
			printf("    Command Type (CTYP): 0x%02x\n", cmd_list->commands[i].ctyp);
			printf("    NVMe-MI Message Type (NMIMT): 0x%02x\n",
				   (cmd_list->commands[i].ctyp >> 3) & 0x0F);
			printf("    Opcode (OPC): 0x%02x\n", cmd_list->commands[i].opc);
		}
	}
}

/**
 * Execute NVMe-MI Read Data Structure command
 * @param dev: NVMe device
 * @param dtyp: Data Structure Type
 * @param portid: Port Identifier (used for certain data structures)
 * @param ctrlid: Controller Identifier (used for certain data structures) 
 * @param iocsi: I/O Command Set Identifier (used for certain data structures)
 * @param csi: Command Slot Identifier
 */
static int nvme_amd_read_data_structure(struct nvme_dev *dev, __u8 dtyp, __u8 portid, 
                                        __u16 ctrlid, __u8 iocsi, __u8 csi)
{
	struct nvme_mi_mi_req_hdr mi_req = {0};
	struct {
		struct nvme_mi_mi_resp_hdr mi_hdr;
		char buffer[4096];
	} resp = { 0 };

	size_t len = 4096;
	int rc = 0;

	/* Set up the MI request header */
	mi_req.opcode = READ_NVME_MI_DATA_STRUCTURE;  /* Opcode 0 for Read NVMe-MI Data Structure */
	
	/* Set up NVMe Management Dword 0 - Figure 109 in spec */
	mi_req.cdw0 = ((__u32)dtyp << 24) |         /* Bits 31:24 - Data Structure Type */
	              ((__u32)portid << 16) |       /* Bits 23:16 - Port Identifier */
	              ((__u32)ctrlid);              /* Bits 15:00 - Controller Identifier */
	
	/* Set up NVMe Management Dword 1 - Figure 110 in spec */
	mi_req.cdw1 = (__u32)iocsi;                 /* Bits 07:00 - I/O Command Set Identifier */

	printf("Sending Read NVMe-MI Data Structure command:\n");
	printf("  Data Structure Type (DTYP): 0x%02x\n", dtyp);
	printf("  Port Identifier (PORTID): 0x%02x\n", portid);
	printf("  Controller Identifier (CTRLID): 0x%04x\n", ctrlid);
	printf("  I/O Command Set Identifier (IOCSI): 0x%02x\n", iocsi);
	printf("  Command Slot Identifier (CSI): 0x%02x\n", csi);

	rc = nvme_mi_mi_xfer(dev->mi.ep, &mi_req, 0, &resp.mi_hdr, &len, csi);
	
	if (rc != 0) {
		printf("ERROR: NVMe-MI transfer failed with error %d\n", rc);
		return rc;
	}

	printf("\nNVMe-MI Response Status: %d\n", resp.mi_hdr.status);
	
	if (resp.mi_hdr.status != STATUS_SUCCESS) {
		printf("ERROR: Command failed with status %d\n", resp.mi_hdr.status);
		return -1;
	}

	/* Parse the Response Data Length from NVMe Management Response - Figure 111 */
	__u16 rdl = resp.mi_hdr.nmresp[0] | (resp.mi_hdr.nmresp[1] << 8);
	printf("Response Data Length (RDL): %u bytes\n", rdl);

	if (rdl == 0) {
		printf("No response data returned\n");
		return 0;
	}

	/* Parse the response data based on Data Structure Type */
	switch (dtyp) {
	case NVME_MI_DS_NVM_SUBSYSTEM_INFO:
		if (rdl >= sizeof(nvm_subsystem_info_t)) {
			parse_nvm_subsystem_info((nvm_subsystem_info_t *)resp.buffer);
		} else {
			printf("ERROR: Insufficient data length for NVM Subsystem Information\n");
		}
		break;
		
	case NVME_MI_DS_PORT_INFO:
		if (rdl >= sizeof(port_info_t)) {
			parse_port_info((port_info_t *)resp.buffer);
		} else {
			printf("ERROR: Insufficient data length for Port Information\n");
		}
		break;
		
	case NVME_MI_DS_CONTROLLER_LIST: {
		/* Controller List is a simple list of 2-byte Controller IDs */
		__u16 *ctrl_list = (__u16 *)resp.buffer;
		int num_controllers = rdl / 2;
		printf("=== Controller List Data Structure ===\n");
		printf("Number of Controllers: %d\n", num_controllers);
		for (int i = 0; i < num_controllers; i++) {
			printf("  Controller %d ID: 0x%04x\n", i, ctrl_list[i]);
		}
		break;
	}
	
	case NVME_MI_DS_CONTROLLER_INFO:
		if (rdl >= sizeof(controller_info_t)) {
			parse_controller_info((controller_info_t *)resp.buffer);
		} else {
			printf("ERROR: Insufficient data length for Controller Information\n");
		}
		break;
		
	case NVME_MI_DS_OPTIONAL_COMMAND_LIST:
		if (rdl >= sizeof(opt_supported_cmd_list_t)) {
			parse_opt_supported_cmd_list((opt_supported_cmd_list_t *)resp.buffer, rdl);
		} else {
			printf("ERROR: Insufficient data length for Optional Command List\n");
		}
		break;
		
	case NVME_MI_DS_MGMT_EP_BUFFER_CMD_SUPPORT_LIST:
		if (rdl >= sizeof(mgmt_ep_buffer_cmd_list_t)) {
			parse_mgmt_ep_buffer_cmd_list((mgmt_ep_buffer_cmd_list_t *)resp.buffer, rdl);
		} else {
			printf("ERROR: Insufficient data length for Management Endpoint Buffer Command List\n");
		}
		break;
		
	default:
		printf("ERROR: Unknown Data Structure Type: 0x%02x\n", dtyp);
		printf("Raw response data (%u bytes):\n", rdl);
		for (int i = 0; i < rdl && i < 256; i++) {
			if (i % 16 == 0) printf("%04x: ", i);
			printf("%02x ", (unsigned char)resp.buffer[i]);
			if (i % 16 == 15) printf("\n");
		}
		if (rdl % 16 != 0) printf("\n");
		break;
	}

	return 0;
}

static int nvme_amd_config_get(struct nvme_dev *dev, __u8 cid, __u8 pid, __u8 csi)
{
	struct nvme_mi_mi_req_hdr mi_req = {0};
	struct {
		struct nvme_mi_mi_resp_hdr mi_hdr;
		char buffer[4096];
	} resp = { 0 };

	size_t len = 4096;
	int rc = 0;

	mi_req.opcode = 4;
	mi_req.cdw0 = ((uint32_t)cid) | ((uint32_t)pid << 24);
	printf("Plugin csi %d\n", csi);
	rc = nvme_mi_mi_xfer(dev->mi.ep, &mi_req, 0, &resp.mi_hdr, &len, csi);
	assert(rc == 0);

	printf("\nSTATUS:%d \n",resp.mi_hdr.status);
	if (cid == 4) {
		struct {
			ae_supp_list_hdr_t *hdr;
			ae_supp_ds_t *ds;
		}ae_resp = {0};

		printf("AEELVER:%d \n",resp.mi_hdr.nmresp[0]);
		ae_resp.hdr = (ae_supp_list_hdr_t *)resp.buffer;
		ae_resp.ds = (ae_supp_ds_t *)(resp.buffer + sizeof(ae_supp_list_hdr_t));
    	
		// Access the structure fields
    	printf("Number of AE Supported DS (NUMAES): %u\n", ae_resp.hdr->num_ae_supp_ds);
    	printf("AE Supported List Version Number (AESLVER): %u\n", ae_resp.hdr->ae_supp_list_ver_num);
    	printf("AE Supported Total Length (AESTL): %u\n", ae_resp.hdr->ae_supp_total_len);
    	printf("AE Supported List Header Length (AESLHL): %u\n", ae_resp.hdr->ae_supp_list_hdr_len);
		printf("********************************************************\n");
		printf("               AE Data Structures to follow             \n");
		printf("********************************************************\n");

		// Loop through each ae_supp_ds structure and print their values
    	for (int i = 0; i < ae_resp.hdr->num_ae_supp_ds; i++) {
        	// Access the structure fields
			printf("********************************************************\n");
        	printf("AE Supported ID (AESI): %u\n", ae_resp.ds->ae_supp_id);
        	printf("AE Supported Enable (AESE): %u\n", ae_resp.ds->ae_supp_enable);
        	printf("AE Supported Length (AESL): %u\n", ae_resp.ds->ae_supp_len);
        	printf("Reserved (RSVD): %u\n", ae_resp.ds->rsvd);
			printf("********************************************************\n");

        	// Move the pointer to the next ae_supp_ds structure
			ae_resp.ds++;
    	}
	} else if (cid == 3) {
		printf("MTU: %d for PORT ID: %d \n",resp.mi_hdr.nmresp[0], pid);
	} else {
		printf("SFREQ: %d for PORT ID: %d \n",resp.mi_hdr.nmresp[0], pid);
	}
	return 0;
}

static int nvme_amd_set_ae(struct nvme_dev *dev, __u8 *enable_list, __u8 *disable_list,
						   __u8 aerd, __u8 aemd, __u8 numaee, __u8 aeelver,
						   __u8 aeetl, __u8 aeelhl, __u8 csi)
{
	struct {
		struct nvme_mi_mi_resp_hdr resp_hdr;
		uint8_t resp_buf[4096];
	} resp = { 0 };

	struct {
		struct nvme_mi_mi_req_hdr req_hdr;
		ae_enable_list_hdr_t aeelh;
		ae_enable_ds_t ae_en_info[35];
	} req = { 0 };

	struct {
		ae_occ_list_hdr_t *occ_list_hdr;
    	ae_occ_hdr_t *occ_hdr;
	} ae_resp = {0};

	uint8_t cid = 4, msg_size = 5;
	size_t len = 4096;
	int rc = 0;
	uint8_t *ae_occ_spec_info;
	uint8_t *ae_occ_ven_spec_info;

	//Opcode for Configuration set
	req.req_hdr.opcode = 3;

	//store cid, retry delay and delay field
	req.req_hdr.cdw0 = ((uint32_t)cid) | ((uint32_t)aerd << 8) | ((uint32_t)aemd << 16);
	printf("cdw0 field value is: %06x\n",req.req_hdr.cdw0);

	//number of AE's to be enabled/disabled
	req.aeelh.num_ae_enable_ds = numaee;
	printf("NUMAEE:: 0x%x\n",req.aeelh.num_ae_enable_ds);
	//version number for testing purpose only
	if (aeelver != 0) {
		req.aeelh.ae_enable_list_ver_num = aeelver;
	}

	//If user defined total length is available then only store it (Only for Testing purpose)
	//otherwise it is based on number of ae's.
	if (aeetl != 0) {
		req.aeelh.ae_enable_total_len = aeetl;
	} else {
		req.aeelh.ae_enable_total_len = (req.aeelh.num_ae_enable_ds * sizeof(ae_enable_ds_t)) + sizeof(ae_enable_list_hdr_t);
	}

	printf(" ae_enable_total_len:: %d\n",req.aeelh.ae_enable_total_len);

	//If user defined header length is available then only store it (Only for Testing purpose)
	//otherwise it is fixed.
	if (aeelhl != 0) {
		req.aeelh.ae_enable_list_hdr_len = aeelhl;
	} else {
		req.aeelh.ae_enable_list_hdr_len = sizeof(ae_enable_list_hdr_t);
	}
	printf(" ae_enable_list_hdr_len:: %d\n",req.aeelh.ae_enable_list_hdr_len);

	// Store elements in enable_list
	int i = 0;
	while (enable_list[i] >= 0 && i < sizeof(enable_list)) {
		req.ae_en_info[i].ae_enable_len = 3;
		req.ae_en_info[i].ae_enable_id = enable_list[i];
		req.ae_en_info[i].ae_enable = 1;
		i++;
	}
	// Store elements in disable_list
	i = 0;
	while (disable_list[i] != 0 && i < sizeof(disable_list)) {
		req.ae_en_info[i].ae_enable_len = 3;
		req.ae_en_info[i].ae_enable_id = disable_list[i];
		req.ae_en_info[i].ae_enable = 0;
		i++;
	}
	//Message size to be sent 
	msg_size = (msg_size + (numaee * sizeof(ae_enable_ds_t)));
	//Send the message for ACK size = 5 if numaee==0 else size = 5 + (numaee * 3)
	rc = nvme_mi_mi_xfer(dev->mi.ep, &req.req_hdr, msg_size, &resp.resp_hdr, &len, csi);
	assert(rc == 0);

	if (resp.resp_hdr.status) {
		int status = parse_mi_resp_status(resp.resp_hdr.status, resp.resp_buf);
		if (status != 1) return -1;
	}
	ae_resp.occ_list_hdr = (ae_occ_list_hdr_t *)resp.resp_buf;
	ae_resp.occ_hdr = (ae_occ_hdr_t *)(resp.resp_buf + sizeof(ae_occ_list_hdr_t));
	
	// Print the fields of ae_occ_list_hdr_t
	printf("ae_occ_list_hdr_t:\n");
	printf("num_ae_occ_ds   (NUMAEO): %02x\n", ae_resp.occ_list_hdr->num_ae_occ_ds);
	printf("ae_occ_list_ver_num (AELVER): %02x\n", ae_resp.occ_list_hdr->ae_occ_list_ver_num);
	printf("ae_occ_list_total_len (AEOLTL): %06x\n", ae_resp.occ_list_hdr->ae_occ_list_total_len);
	printf("ae_occ_list_overflow (AEOLO): %01x\n", ae_resp.occ_list_hdr->ae_occ_list_overflow);
	printf("ae_occ_list_hdr_len (AEOLHL): %02x\n", ae_resp.occ_list_hdr->ae_occ_list_hdr_len);
	printf("aem_retry_count (AEMRC): %01x\n", ae_resp.occ_list_hdr->aem_retry_count);
	printf("aem_generation_number (AEMGN): %01x\n", ae_resp.occ_list_hdr->aem_generation_number);
	for (int i = 1; i <= ae_resp.occ_list_hdr->num_ae_occ_ds; i++ ) {
		// Print the fields of ae_occ_hdr_t
	    printf("ae_occ_hdr_t:\n");
		printf("ae_occ_hdr_len (AELHLEN): %02x\n", ae_resp.occ_hdr->ae_occ_hdr_len);
		printf("ae_occ_spec_info_len (AEOSIL): %02x\n", ae_resp.occ_hdr->ae_occ_spec_info_len);
		printf("ae_occ_ven_spec_info_len (AEOVSIL): %02x\n", ae_resp.occ_hdr->ae_occ_ven_spec_info_len);
		// Print the fields of ae_occ_uniq_id
		printf("ae_occ_uniq_id_t:\n");
		printf("ae_occ_id (AEOI): %02x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id);
		printf("ae_occ_scope_id_info (AEOCIDI): %08x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info);
		printf("ae_occ_scope (AESS): %01x\n", ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope);
		printf("rsvd: %02x\n", ae_resp.occ_hdr->ae_occ_uniq_id.rsvd);
		ae_occ_spec_info = (uint8_t*)(ae_resp.occ_hdr);
		ae_occ_spec_info = ae_occ_spec_info + 9;
				
		if (ae_resp.occ_hdr->ae_occ_spec_info_len && !(ae_resp.occ_hdr->ae_occ_ven_spec_info_len)) {
			parse_ae_occ_spec_info(ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id, 
								   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info, 
								   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope, 
								   *(ae_occ_spec_info));
		} else if (!(ae_resp.occ_hdr->ae_occ_spec_info_len) && ae_resp.occ_hdr->ae_occ_ven_spec_info_len) {
			parse_ae_occ_ven_spec_info(ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_id, 
									   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope_id_info, 
									   ae_resp.occ_hdr->ae_occ_uniq_id.ae_occ_scope, 
									   ae_occ_spec_info);
		} else if ((ae_resp.occ_hdr->ae_occ_spec_info_len) && (ae_resp.occ_hdr->ae_occ_ven_spec_info_len)) {
			   printf("check if occ_hdr->ae_occ_uniq_id.ae_occ_id is <C0-FF>, if yes then raise error\n");
		} else {printf("No AE occurance data structure present\n");}
		if (ae_resp.occ_hdr->ae_occ_spec_info_len){
			ae_occ_spec_info = ++ae_occ_spec_info;
		} else {
			ae_occ_spec_info += 7;
		}
		ae_resp.occ_hdr = ae_occ_spec_info;
	}
		
	return 0;
}

static int config_ae(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Configuration set NVMe-MI command to enable/disable Async Event.\n"
			"Return results.\n");

    const char *aerd = "Retry Delay (required)";
	const char *aemd = "Delay";
	const char *numaee = "Number of AE to be configured";
    const char *enable_aeid_list = "Comma seprated event ID List to be enabled";
	const char *disable_aeid_list = "Comma seprated event ID List to be disabled";
	const char * aeelver = "AE Event List Version";
	const char * aeetl = "AE Event List Total length";
	const char * aeelhl = "AE Event List Header length";
	const char *csi = "Command slot identifier";

	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	__u8 enable_list[34] = {0};
	__u8 disable_list[34] = {0};

	struct ae_config cfg = {
		.enable_aeid_list = "",
		.disable_aeid_list = "",
		.aerd = 0,
		.aemd = 0,
		.numaee = 0,
		.aeelver = 0,
		.aeetl = 0,
		.aeelhl = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		  OPT_UINT("aerd", 'r', &cfg.aerd, aerd),
		  OPT_UINT("aemd", 'd', &cfg.aemd, aemd),
		  OPT_UINT("numaee", 'n', &cfg.numaee, numaee),
		  OPT_LIST("enable_aeid_list", 'e', &cfg.enable_aeid_list, enable_aeid_list),
		  OPT_LIST("disable_aeid_list", 'D', &cfg.disable_aeid_list, disable_aeid_list),
		  OPT_UINT("aeelver", 'v', &cfg.aeelver, aeelver),
		  OPT_UINT("aeetl", 't', &cfg.aeetl, aeetl),
		  OPT_UINT("aeelhl", 'h', &cfg.aeelhl, aeelhl),
		  OPT_UINT("csi", 'c', &cfg.csi, csi),
		  OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);
	if (cfg.numaee != 0) {
		if (strlen(cfg.enable_aeid_list) > 0) {
			status = argconfig_parse_comma_sep_array_u8(cfg.enable_aeid_list,
									  enable_list,
									  sizeof(enable_list));
			if (status < 0) {
				nvme_show_error("%s: %s", __func__, nvme_strerror(errno));
				return status;
			}
		}

		if (strlen(cfg.disable_aeid_list) > 0) {
			status = argconfig_parse_comma_sep_array_u8(cfg.disable_aeid_list,
								  disable_list,
								  sizeof(disable_list));
			if (status < 0) {
				nvme_show_error("%s: %s", __func__, nvme_strerror(errno));
				return status;
			}
		}
		if (strlen(cfg.enable_aeid_list) == 0 && strlen(cfg.disable_aeid_list) == 0) {
			nvme_show_error("%s: Both enable and disable AEID lists are empty", __func__);
			return -EINVAL;
		}
	}
	err = nvme_amd_set_ae(dev, enable_list, disable_list,
						 cfg.aerd, cfg.aemd,
						 cfg.numaee, cfg.aeelver,
						 cfg.aeetl, cfg.aeelhl, cfg.csi);

	gettimeofday(&end_time, NULL);
	
	return err;
}

static int config_get(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Configuration get NVMe-MI command to retrieve Async Event status.\n"
			"Return results."
		);

    const char *cid = "Configuration identifier (required)";
	const char *pid = "Port ID";
	const char *csi = "Command slot identifier";
	
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	struct get_config cfg = {
		.cid = 0,
		.pid = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		  OPT_UINT("cid", 'c', &cfg.cid, cid),
		  OPT_UINT("pid", 'p', &cfg.pid, pid),
		  OPT_UINT("csi", 'c', &cfg.csi, csi),
		  OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);
	printf("config_get csi %d\n", cfg.csi);
	err = nvme_amd_config_get(dev, cfg.cid, cfg.pid, cfg.csi);
						 
	gettimeofday(&end_time, NULL);
	
	return err;
}

static int ctrl_primitive(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    const char *desc =(
			"Send a Control primitive NVMe-MI command[abort|pause|resume|get-state|replay].\n"
			"Return results."
		);

    const char *action = "Control primitive action to be taken (required)";
	const char *csi = "Command slot identifier";
	const char *cpsp = "Control Primitive specific parameter";
	const char *tag = "Tag value";
	
	
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	int rc = 0;
	uint8_t opcode = 0xff;
	uint16_t cpsr;
	
	struct config {
		char		*action;
		__u8 csi;
		__u8 cpsp;
		__u8 tag;
	};

	struct config cfg = {
		.action = "",
		.csi = 0,
		.cpsp = 0,
		.tag = 0,
	};

	OPT_ARGS(opts) = {
		  OPT_STR("action", 'a', &cfg.action, action),
		  OPT_UINT("csi", 'c', &cfg.csi, csi),
		  OPT_UINT("cpsp", 's', &cfg.cpsp, cpsp),
		  OPT_UINT("tag", 't', &cfg.tag, tag),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);


	static const struct {
		const char *name;
		uint8_t opcode;
	} control_actions[] = {
		{ "pause", nvme_mi_control_opcode_pause },
		{ "resume", nvme_mi_control_opcode_resume },
		{ "abort", nvme_mi_control_opcode_abort },
		{ "get-state", nvme_mi_control_opcode_get_state },
		{ "replay", nvme_mi_control_opcode_replay },
	};

	static const char * const slot_state[] = {
		"Idle",
		"Receive",
		"Process",
		"Transmit",
	};

	static const char * const cpas_state[] = {
		"Command aborted after processing completed or no command to abort",
		"Command aborted before processing began",
		"Command processing partially completed",
		"Reserved",
	};

	for (int i = 0; i < 5; i++) {
		if (!strcmp(cfg.action, control_actions[i].name)) {
			opcode = control_actions[i].opcode;
			break;
		}
	}

	if (opcode == 0xff) {
		fprintf(stderr, "invalid action specified: %s\n", action);
		return -1;
	}

	
	printf("BHUPI : TAG VALUE %d",cfg.tag);	
	rc = nvme_mi_control(dev->mi.ep, opcode, cfg.cpsp, &cpsr, cfg.csi, cfg.tag); /* cpsp reserved in example */
	if (rc) {
		printf("Response: %d\n", rc);
		warn("can't perform primitive control command");
		return -1;
	}

	switch (opcode) {
	case nvme_mi_control_opcode_pause:
		printf(" cpsr : %#x\n", cpsr);
		printf(" PFSS0: %s\n", (cpsr & (1 << 0)) ? "Yes" : "No");
		printf(" PFSS1: %s\n", (cpsr & (1 << 1)) ? "Yes" : "No");
		break;
	case nvme_mi_control_opcode_resume:
		printf(" RES:%#x\n", cpsr);
		break;
	case nvme_mi_control_opcode_abort:
		printf(" cpsr: %#x\n", cpsr);
		printf(" CAS: %s\n", cpas_state[cpsr & 0x3]);
		break;
	case nvme_mi_control_opcode_get_state:
		printf(" cspr: %#x\n", cpsr);
		printf(" SSTA: %s\n", slot_state[cpsr & 0x3]);
		printf(" CMNICS: %s\n", (cpsr & (1 << 4)) ? "Yes" : "No");
		printf(" BMICE: %s\n", (cpsr & (1 << 5)) ? "Yes" : "No");
		printf(" UTUNT: %s\n", (cpsr & (1 << 6)) ? "Yes" : "No");
		printf(" BHVS: %s\n", (cpsr & (1 << 7)) ? "Yes" : "No");
		printf(" UDSTID: %s\n", (cpsr & (1 << 8)) ? "Yes" : "No");
		printf(" ITU: %s\n", (cpsr & (1 << 9)) ? "Yes" : "No");
		printf(" UMEP: %s\n", (cpsr & (1 << 10)) ? "Yes" : "No");
		printf(" OSPSN: %s\n", (cpsr & (1 << 11)) ? "Yes" : "No");
		printf(" BUEMT: %s\n", (cpsr & (1 << 12)) ? "Yes" : "No");
		printf(" BPOPL: %s\n", (cpsr & (1 << 13)) ? "Yes" : "No");
		printf(" NSSRO: %s\n", (cpsr & (1 << 14)) ? "Yes" : "No");
		printf(" PFLG: %s\n", (cpsr & (1 << 15)) ? "Yes" : "No");
		break;
	case nvme_mi_control_opcode_replay:
		printf(" REP: %#x\n", cpsr);
		break;
	default:
		/* unreachable */
		break;
	}
	//err = nvme_amd_config_get(dev, cfg.cid, cfg.pid);
						 
	gettimeofday(&end_time, NULL);
	
	return err;
}

#if 0
#define MAX_BATCH_COMMANDS 100
#define MAX_COMMAND_LENGTH 512

struct nvme_mi_transport_mctp_async {
    int net;
    __u8 eid;
    int sd;
    void *resp_buf;
    size_t resp_buf_size;
    pthread_t poll_thread;
    bool stop_polling;
};

static void *poll_socket(void *arg) {
    struct nvme_mi_transport_mctp_async *mctp = (struct nvme_mi_transport_mctp_async *)arg;
    struct pollfd pollfds[1];
    struct msghdr resp_msg;
    struct iovec resp_iov[1];
    ssize_t len;

    pollfds[0].fd = mctp->sd;
    pollfds[0].events = POLLIN;

    while (!mctp->stop_polling) {
        int rc = poll(pollfds, 1, 1000); // Poll with a timeout of 60 seconds
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (rc == 0) // Timeout
            continue;

        if (pollfds[0].revents & POLLIN) {
            memset(&resp_msg, 0, sizeof(resp_msg));
            resp_iov[0].iov_base = mctp->resp_buf;
            resp_iov[0].iov_len = mctp->resp_buf_size;
            resp_msg.msg_iov = resp_iov;
            resp_msg.msg_iovlen = 1;

            len = recvmsg(mctp->sd, &resp_msg, MSG_DONTWAIT);
            if (len < 0) {
		    printf("Error receiving message\n");
                continue;
            }

		printf("Received message: ");
		for (size_t i = 0; i < len; i++) {
			printf("%02x ", ((unsigned char *)mctp->resp_buf)[i]);
		}
		printf("\n");
            // Process the message here
        }
    }

    return NULL;
}

static int create_mctp_listener_thread(int net, __u8 eid, size_t resp_buf_size) {
	struct nvme_mi_transport_mctp_async *mctp = malloc(sizeof(struct nvme_mi_transport_mctp_async));
	if (!mctp) {
		fprintf(stderr, "Error: Failed to allocate memory for MCTP transport.\n");
		return -1;
	}

	mctp->net = net;
	mctp->eid = eid;
	mctp->resp_buf_size = resp_buf_size;
	mctp->resp_buf = malloc(resp_buf_size);
	if (!mctp->resp_buf) {
		fprintf(stderr, "Error: Failed to allocate memory for response buffer.\n");
		free(mctp);
		return -1;
	}

	mctp->sd = socket(AF_MCTP, SOCK_DGRAM, 0);
	if (mctp->sd < 0) {
		fprintf(stderr, "Error: Failed to create MCTP socket: %m\n");
		free(mctp->resp_buf);
		free(mctp);
		return -1;
	}

	 int sockfd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("MCTP socket creation failed");
        exit(1);
    }

	struct sockaddr_mctp addr = {
		.smctp_family = AF_MCTP,
		.smctp_network = net,
    		.smctp_addr.s_addr = DEFAULT_MCTP_EID,
    		.smctp_type = MCTP_TYPE_NVME | MCTP_TYPE_MIC,
	};

	if (bind(mctp->sd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Error: Failed to bind MCTP socket: %m\n");
		close(mctp->sd);
		free(mctp->resp_buf);
		free(mctp);
		return -1;
	}

	mctp->stop_polling = false;

	if (pthread_create(&mctp->poll_thread, NULL, poll_socket, mctp) != 0) {
		fprintf(stderr, "Error: Failed to create polling thread.\n");
		close(mctp->sd);
		free(mctp->resp_buf);
		free(mctp);
		return -1;
	}

	printf("MCTP listener thread created successfully.\n");
	return 0;
}

struct batch_command {
    char command[MAX_COMMAND_LENGTH];
    char options[MAX_COMMAND_LENGTH];
    __u8 opcode;
    __u8 flags;
    __u32 nsid;
    __u32 cdw10;
    __u32 cdw11;
    __u32 cdw12;
    __u32 cdw13;
    __u32 cdw14;
    __u32 cdw15;
    __u32 data_len;
    void *data;
    __u32 metadata_len;
    void *metadata;
    __u32 timeout_ms;
    __u8 csi;
    __u32 offset;
};

static struct batch_command batch_commands[MAX_BATCH_COMMANDS];
static int batch_command_count = 0;

static int add_batch_command(const char *command, const char *options) {
    if (batch_command_count >= MAX_BATCH_COMMANDS) {
        fprintf(stderr, "Error: Maximum batch command limit reached.\n");
        return -1;
    }

    strncpy(batch_commands[batch_command_count].command, command, MAX_COMMAND_LENGTH - 1);
    strncpy(batch_commands[batch_command_count].options, options, MAX_COMMAND_LENGTH - 1);

    // Parse options for admin-passthru
    if (strcmp(command, "admin-passthru") == 0) {
        sscanf(options,
               "--opcode=%hhx --flags=%hhx --nsid=%u --cdw10=%x --cdw11=%x --cdw12=%x --cdw13=%x --cdw14=%x --cdw15=%x --data_len=%u --timeout_ms=%u --csi=%hhx --offset=%u",
               &batch_commands[batch_command_count].opcode,
               &batch_commands[batch_command_count].flags,
               &batch_commands[batch_command_count].nsid,
               &batch_commands[batch_command_count].cdw10,
               &batch_commands[batch_command_count].cdw11,
               &batch_commands[batch_command_count].cdw12,
               &batch_commands[batch_command_count].cdw13,
               &batch_commands[batch_command_count].cdw14,
               &batch_commands[batch_command_count].cdw15,
               &batch_commands[batch_command_count].data_len,
               &batch_commands[batch_command_count].timeout_ms,
               &batch_commands[batch_command_count].csi,
               &batch_commands[batch_command_count].offset);
    }

    batch_command_count++;
    return 0;
}

static int execute_batch_commands(struct nvme_dev *dev) {
    for (int i = 0; i < batch_command_count; i++) {
        printf("Executing command %d: %s %s\n", i + 1, batch_commands[i].command, batch_commands[i].options);

        if (strcmp(batch_commands[i].command, "admin-passthru") == 0) {
            int rc = nvme_cli_admin_batch_passthru(dev,
                                             batch_commands[i].opcode,
                                             batch_commands[i].flags,
                                             0, // reserved
                                             batch_commands[i].nsid,
                                             0, // cdw2
                                             0, // cdw3
                                             batch_commands[i].cdw10,
                                             batch_commands[i].cdw11,
                                             batch_commands[i].cdw12,
                                             batch_commands[i].cdw13,
                                             batch_commands[i].cdw14,
                                             batch_commands[i].cdw15,
                                             batch_commands[i].data_len,
                                             batch_commands[i].data,
                                             batch_commands[i].metadata_len,
                                             batch_commands[i].metadata,
                                             batch_commands[i].timeout_ms,
                                             NULL, // result
                                             batch_commands[i].csi,
                                             batch_commands[i].offset);
            if (rc) {
                fprintf(stderr, "Error: admin-passthru command failed with return code %d.\n", rc);
                return rc;
            }
        } else {
			char full_command[256];
			snprintf(full_command, sizeof(full_command), "nvme amd %s %s", batch_commands[i].command, batch_commands[i].options);
    		system(full_command); // Executes the command in the shell
			return 0;
        }
    }
    return 0;
}
static int batch_mode(int argc, char **argv, struct command *cmd, struct plugin *plugin) {
    const char *desc = (
        "Enter batch mode to queue multiple NVMe commands with options.\n"
        "Commands will be executed sequentially after exiting batch mode.\n"
    );

    printf("%s\n", desc);
    printf("Enter commands in the format: <command> <options>\n");
    printf("Type 'run' to execute the batch or 'exit' to cancel.\n");

    char input[MAX_COMMAND_LENGTH * 2];
    _cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
    int err = parse_and_open(&dev, argc, argv, desc, NULL);
    printf("\nBHUPI 1\n");
    if (err)
        return err;
    create_mctp_listener_thread(1, 9, 4096);
    while (1) {
        printf("batch> ");
        if (!fgets(input, sizeof(input), stdin)) {
            fprintf(stderr, "Error reading input.\n");
            return -1;
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "run") == 0) {
            int rc = execute_batch_commands(dev);
			if (rc == 0) {
				printf("Batch commands executed successfully.\n");
			}
        } else if (strcmp(input, "exit") == 0) {
            printf("Exiting batch mode.\n");
			//nvme_mi_async_stop_polling(dev->mi.ep);
            return 0;
        } else {
            char *command = strtok(input, " ");
            char *options = strtok(NULL, "");

            if (!command) {
                fprintf(stderr, "Error: Invalid input format.\n");
                continue;
            }

            if (add_batch_command(command, options ? options : "") != 0) {
                return -1;
            }
        }
    }
}
#endif

static int set_health_status_change(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc =(
		"Send a Configuration set NVMe-MI command to clear selected status bits in the Composite Controller Status Flags field.\n");

	const char *rdy = "Ready (bit 0)";
    const char *cfs = "Controller Fatal Status (bit 1)";
    const char *shst = "Shutdown Status (bit 2)";
    const char *nssro = "NVM Subsystem Reset Occurred (bit 3)";
    const char *ceco = "Controller Enable Change Occurred (bit 4)";
    const char *nac = "Namespace Attribute Changed (bit 5)";
    const char *fa = "Firmware Activated (bit 6)";
    const char *cschng = "Controller Status Change (bit 7)";
    const char *ctemp = "Composite Temperature (bit 8)";
    const char *pdlu = "Percentage Used (bit 9)";
    const char *spare = "Available Spare (bit 10)";
    const char *cwarn = "Critical Warning (bit 11)";
    const char *tcida = "Telemetry Controller-Initiated Data Available (bit 12)";
	
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	uint8_t cid = 2, csi = 0;
	size_t len = 4096;

	struct health_status_change cfg ={
		.rdy = 0,
		.cfs = 0,
		.shst = 0,
		.nssro = 0,
		.ceco = 0,
		.nac = 0,
		.fa = 0,	
		.cschng = 0,
		.ctemp = 0,
		.pdlu = 0,
		.spare = 0,
		.cwarn = 0,
		.tcida = 0,
		.reserved = 0,
	};


	OPT_ARGS(opts) = {
	  	OPT_FLAG("rdy", 'r', &cfg.rdy, rdy),
	  	OPT_FLAG("cfs", 'c', &cfg.cfs, cfs),
	  	OPT_FLAG("shst", 's',&cfg.shst, shst),
	  	OPT_FLAG("nssro", 'n', &cfg.nssro, nssro),
	  	OPT_FLAG("ceco", 'e', &cfg.ceco, ceco),
	  	OPT_FLAG("nac", 'a', &cfg.nac, nac),
		OPT_FLAG("fa", 'f', &cfg.fa, fa),
		OPT_FLAG("cschng", 'h', &cfg.cschng, cschng),
		OPT_FLAG("ctemp", 't', &cfg.ctemp, ctemp),
		OPT_FLAG("pdlu", 'p', &cfg.pdlu, pdlu),
		OPT_FLAG("spare", 'z', &cfg.spare, spare),
		OPT_FLAG("cwarn", 'w', &cfg.cwarn, cwarn),
		OPT_FLAG("tcida", 'i', &cfg.tcida, tcida),
	  	OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);
	struct {
		struct nvme_mi_mi_resp_hdr resp_hdr;
		uint8_t resp_buf[4096];
	} resp = { 0 };

	struct nvme_mi_mi_req_hdr req_hdr = {0};

	//Opcode for Configuration set
	req_hdr.opcode = 3;

	//store cid, retry delay and delay field
	req_hdr.cdw0 = ((uint32_t)cid);

	// Create a uint32 to store all the flag variables
	uint32_t flag_data = 0;

	// Pack all the flag bits into the uint32
	flag_data |= cfg.rdy ? 1 : 0;
	flag_data |= (cfg.cfs ? 1 : 0) << 1;
	flag_data |= (cfg.shst ? 1 : 0) << 2;
	flag_data |= (cfg.nssro ? 1 : 0) << 3;
	flag_data |= (cfg.ceco ? 1 : 0) << 4;
	flag_data |= (cfg.nac ? 1 : 0) << 5;
	flag_data |= (cfg.fa ? 1 : 0) << 6;
	flag_data |= (cfg.cschng ? 1 : 0) << 7;
	flag_data |= (cfg.ctemp ? 1 : 0) << 8;
	flag_data |= (cfg.pdlu ? 1 : 0) << 9;
	flag_data |= (cfg.spare ? 1 : 0) << 10;
	flag_data |= (cfg.cwarn ? 1 : 0) << 11;
	flag_data |= (cfg.tcida ? 1 : 0) << 12;
	// Bits 13-31 are reserved, so we leave them as 0

	req_hdr.cdw1 = flag_data;

	int rc = 0;
	//Send the message 
	rc = nvme_mi_mi_xfer(dev->mi.ep, &req_hdr, 0, &resp.resp_hdr, &len, csi);
	assert(rc == 0);

	if (resp.resp_hdr.status) {
		int status = parse_mi_resp_status(resp.resp_hdr.status, resp.resp_buf);
		if (status != 1) return -1;
	}

gettimeofday(&end_time, NULL);

return err;
	
}

/**  
 * Helper function to interpret composite temperature value  
 * Returns the temperature in degrees Celsius.  
 * Returns INT_MIN for special values like data too old or sensor failure  
 */  
int interpret_composite_temperature(uint8_t temp_value) {  
    if (temp_value <= 0x7E) {  
        /* 0-126°C */  
        return (int)temp_value;  
    } else if (temp_value == 0x7F) {  
        /* ≥ 127°C */  
        return 127;  
    } else if (temp_value == 0x80) {  
        /* Temperature data is greater than 5s old */  
        return INT_MIN;  
    } else if (temp_value == 0x81) {  
        /* Temperature data not accurate due to sensor failure */  
        return INT_MIN;  
    } else if (temp_value >= 0x82 && temp_value <= 0xC3) {  
        /* Reserved */  
        return INT_MIN;  
    } else if (temp_value == 0xC4) {  
        /* ≤ -60°C */  
        return -60;  
    } else {  
        /* -59°C to -1°C (two's complement) */  
        /* Convert from two's complement */  
        return (int)((int8_t)temp_value);  
    }  
}

/**  
 * Helper function to print the NVM Subsystem Health Data Structure  
 */  
void print_nvm_subsystem_health(const nvm_subsystem_health_data_structure_t *health) {  
    printf("NVM Subsystem Health Data Structure:\n");  
      
    printf("    NVM Subsystem Status: 0x%02X\n", health->nvm_subsystem_status.raw);  
    printf("    AEM Transmission Failure: %d\n", health->nvm_subsystem_status.bits.aem_transmission_failure);  
    printf("    Sanitize Failure Mode: %d\n", health->nvm_subsystem_status.bits.sanitize_failure_mode);  
    printf("    Drive Functional: %d\n", health->nvm_subsystem_status.bits.drive_functional);  
    printf("    Reset Not Required: %d\n", health->nvm_subsystem_status.bits.reset_not_required);  
    printf("    Port 0 PCIe Link Active: %d\n", health->nvm_subsystem_status.bits.port0_pcie_link_active);  
    printf("    Port 1 PCIe Link Active: %d\n", health->nvm_subsystem_status.bits.port1_pcie_link_active);  
      
    printf("  SMART Warnings: 0x%02X\n", health->smart_warnings);  
    printf("  Composite Temperature: 0x%02X", health->composite_temperature);  
    int temp = interpret_composite_temperature(health->composite_temperature);  
    if (temp != INT_MIN) {  
        printf(" (%d°C)\n", temp);  
    } else if (health->composite_temperature == 0x80) {  
        printf(" (Data > 5s old)\n");  
    } else if (health->composite_temperature == 0x81) {  
        printf(" (Sensor failure)\n");  
    } else {  
        printf(" (Special value)\n");  
    }  
      
    printf("  Percentage Drive Life Used: %d%%\n", health->percentage_drive_life_used);  
	//printf("  Composite Controller Status: 0x%04X\n", *(uint16_t *)health->ccs);  
	printf("    Ready (RDY): %d\n", health->flags.bits.ready);  
	printf("    Controller Fatal Status (CFS): %d\n", health->flags.bits.controller_fatal_status);  
	printf("    Shutdown Status (SHST): %d\n", health->flags.bits.shutdown_status);  
	printf("    NVM Subsystem Reset Occurred (NSSRO): %d\n", health->flags.bits.nvm_subsystem_reset_occurred);  
	printf("    Controller Enable Change Occurred (CECO): %d\n", health->flags.bits.controller_enable_change_occurred);  
	printf("    Namespace Attribute Changed (NAC): %d\n", health->flags.bits.namespace_attribute_changed);  
	printf("    Firmware Activated (FA): %d\n", health->flags.bits.firmware_activated);  
	printf("    Controller Status Change (CSCHNG): %d\n", health->flags.bits.controller_status_change);  
	printf("    Composite Temperature (CTEMP): %d\n", health->flags.bits.composite_temperature_change);  
	printf("    Percentage Used (PDLU): %d\n", health->flags.bits.percentage_used);  
	printf("    Available Spare (SPARE): %d\n", health->flags.bits.available_spare);  
	printf("    Critical Warning (CWARN): %d\n", health->flags.bits.critical_warning);  
	printf("    Telemetry Controller-Initiated Data Available (TCIDA): %d\n", health->flags.bits.telemetry_controller_initiated_data_available);  
} 

static int nvm_ss_hlth_stat_poll(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc =(
		"Send a NVM Subsystem Health Status Poll change\n");

	const char *cs = "Clear Status (bit 31)";
    
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	uint8_t csi = 0;
	size_t len = 4096;

	struct nvm_ss_hlth_stat_poll {
		uint32_t rsvd;
		__u8 cs;
	};

	struct nvm_ss_hlth_stat_poll cfg = {
		.rsvd = 0,
		.cs = 0,
	};
	struct {
		struct nvme_mi_mi_resp_hdr resp_hdr;
		char resp_buf[4096];
	} resp = { 0 };

	struct nvme_mi_mi_req_hdr req_hdr = {0};

	OPT_ARGS(opts) = {
	  	OPT_UINT("cs", 'c', &cfg.cs, cs),
	  	OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	gettimeofday(&start_time, NULL);
	
	//Opcode for NVM Subsystem Health Status Poll
	req_hdr.opcode = 1;
	if (cfg.cs) {
		req_hdr.cdw1 = (1 << 31);
	}
	printf("cdw1 field value is: %06x\n",req_hdr.cdw1);

	int rc = 0;	
	//Send the message 
	rc = nvme_mi_mi_xfer(dev->mi.ep, &req_hdr, 0, &resp.resp_hdr, &len, csi);
	assert(rc == 0);

	if (resp.resp_hdr.status) {
		int status = parse_mi_resp_status(resp.resp_hdr.status, resp.resp_hdr.nmresp);
		if (status != 1) return -1;
	} else {
		nvm_subsystem_health_data_structure_t *nshds = (nvm_subsystem_health_data_structure_t *)resp.resp_buf;
		// Print the NVM Subsystem Health Data Structure
		print_nvm_subsystem_health(nshds);

	}

	gettimeofday(&end_time, NULL);

	return err;
}

/**  
 * Controller Health Status Changed Flags (CHSCF)  
 * As defined in the NVMe Management Interface Specification, Revision 2.0  
 */  
#if 0
typedef struct {  
    union {  
        uint16_t raw;  
        struct {  
            uint16_t ready : 1;                              // Bit 0: Ready (RDY)  
            uint16_t controller_fatal_status : 1;            // Bit 1: Controller Fatal Status (CFS)  
            uint16_t shutdown_status : 1;                    // Bit 2: Shutdown Status (SHST)  
            uint16_t reserved2 : 1;                          // Bit 3: Reserved  
            uint16_t nvm_subsystem_reset_occurred : 1;       // Bit 4: NVM Subsystem Reset Occurred (NSSRO)  
            uint16_t controller_enable_change_occurred : 1;  // Bit 5: Controller Enable Change Occurred (CECO)  
            uint16_t namespace_attribute_changed : 1;        // Bit 6: Namespace Attribute Changed (NAC)  
            uint16_t firmware_activated : 1;                 // Bit 7: Firmware Activated (FA)  
            uint16_t controller_status_change : 1;           // Bit 8: Controller Status Change (CSTS)  
            uint16_t composite_temperature_change : 1;       // Bit 9: Composite Temperature Change (CTEMP)  
            uint16_t percentage_used : 1;                    // Bit 10: Percentage Used (PDLU)  
            uint16_t available_spare : 1;                    // Bit 11: Available Spare (SPARE)  
            uint16_t critical_warning : 1;                   // Bit 12: Critical Warning (CWARN)  
            uint16_t telemetry_controller_initiated_data_available : 1; // Bit 13: TCIDA  
            uint16_t reserved1 : 2;                         // Bits 14-15: Reserved  
        } bits;  
    } flags;  
} controller_health_status_changed_flags_t;
#endif
/**  
 * Controller Health Data Structure (CHDS)  
 * As defined in the NVMe Management Interface Specification, Revision 2.0  
 */  
typedef struct {  
    // Bytes 0-1: Controller Identifier (CTLID)  
    uint16_t controller_identifier;  
  
    // Bytes 2-3: Controller Status (CSTS)  
    union {  
        uint16_t raw;  
        struct {
	    uint8_t ready : 1;
	    uint8_t controller_fatal_status : 1; // Bit 1: Controller Fatal Status (CFS)
            uint8_t shutdown_status : 2;             // Bits 2-3: Shutdown Status (SHST)  
            uint8_t nvm_subsystem_reset_occurred : 1;// Bit 4: NVM Subsystem Reset Occurred (NSSRO)  
            uint8_t controller_enable_change_occurred : 1; // Bit 5: Controller Enable Change Occurred (CECO)  
            uint8_t namespace_attribute_changed : 1;  // Bit 6: Namespace Attribute Changed (NAC)  
            uint8_t firmware_activated : 1;          // Bit 7: Firmware Activated (FA)  
            uint8_t telemetry_controller_initiated_data_available : 1; // Bit 8: TCIDA  
            uint8_t reserved1 : 7;                   // Bits 9-15: Reserved  
        } bits;  
    } controller_status;  
  
    // Bytes 4-5: Composite Temperature (CTEMP) in Kelvins  
    uint16_t composite_temperature;  
  
    // Byte 6: Percentage Used (PDLU)  
    uint8_t percentage_used;  
  
    // Byte 7: Available Spare (SPARE) - Normalized percentage (0%-100%)  
    uint8_t available_spare;  
  
    // Byte 8: Critical Warning (CWARN)  
    union {  
        uint8_t raw;  
        struct {  
            uint8_t spare_threshold : 1;                     // Bit 0: Spare Threshold (ST)  
            uint8_t temperature_above_or_under_threshold : 1;// Bit 1: Temperature Above or Under Threshold (TAUT)  
            uint8_t reliability_degraded : 1;                // Bit 2: Reliability Degraded (RD)  
            uint8_t read_only : 1;                           // Bit 3: Read Only (RO)  
            uint8_t volatile_memory_backup_failed : 1;       // Bit 4: Volatile Memory Backup Failed (VMBF)  
            uint8_t persistent_memory_region_error : 1;      // Bit 5: Persistent Memory Region Error (PMRE)  
            uint8_t reserved2 : 2;                           // Bits 6-7: Reserved  
        } bits;  
    } critical_warning;  
  
    // Bytes 9-10: Controller Health Status Changed (CHSC)
    union {
        uint8_t raw[2];
        struct {
            uint16_t ready : 1;                              // Bit 0: Ready (RDY)
            uint16_t controller_fatal_status : 1;            // Bit 1: Controller Fatal Status (CFS)
            uint16_t shutdown_status : 1;                    // Bit 2: Shutdown Status (SHST)
            uint16_t reserved2 : 1;                          // Bit 3: Reserved
            uint16_t nvm_subsystem_reset_occurred : 1;       // Bit 4: NVM Subsystem Reset Occurred (NSSRO)
            uint16_t controller_enable_change_occurred : 1;  // Bit 5: Controller Enable Change Occurred (CECO)
            uint16_t namespace_attribute_changed : 1;        // Bit 6: Namespace Attribute Changed (NAC)
            uint16_t firmware_activated : 1;                 // Bit 7: Firmware Activated (FA)
            uint16_t controller_status_change : 1;           // Bit 8: Controller Status Change (CSTS)
            uint16_t composite_temperature_change : 1;       // Bit 9: Composite Temperature Change (CTEMP)
            uint16_t percentage_used : 1;                    // Bit 10: Percentage Used (PDLU)
            uint16_t available_spare : 1;                    // Bit 11: Available Spare (SPARE)
            uint16_t critical_warning : 1;                   // Bit 12: Critical Warning (CWARN)
            uint16_t telemetry_controller_initiated_data_available : 1; // Bit 13: TCIDA
            uint16_t reserved1 : 2;                         // Bits 14-15: Reserved
        } bits;
    } flags;

	//controller_health_status_changed_flags_t controller_health_status_changed;
  
    // Bytes 11-15: Reserved  
    uint8_t reserved3[5];  
  
}  __attribute__((packed)) controller_health_data_structure_t;  
  
/**  
 * Helper function to display the contents of the Controller Health Data Structure  
 */  
void print_controller_health_data(const controller_health_data_structure_t *chds) {  
    printf("Controller Health Data Structure:\n");  
  
    printf("  Controller Identifier: 0x%04X\n", chds->controller_identifier);  
    printf("  Controller Status: 0x%04X\n", chds->controller_status.raw);  
    printf("  Ready: %d\n", chds->controller_status.bits.ready);
    printf("  Controller Fatal Status: %d\n", chds->controller_status.bits.controller_fatal_status);
    printf("    Shutdown Status: %d\n", chds->controller_status.bits.shutdown_status);  
    printf("    NVM Subsystem Reset Occurred: %d\n", chds->controller_status.bits.nvm_subsystem_reset_occurred);  
    printf("    Controller Enable Change Occurred: %d\n", chds->controller_status.bits.controller_enable_change_occurred);  
    printf("    Namespace Attribute Changed: %d\n", chds->controller_status.bits.namespace_attribute_changed);  
    printf("    Firmware Activated: %d\n", chds->controller_status.bits.firmware_activated);  
    printf("    Telemetry Controller-Initiated Data Available: %d\n", chds->controller_status.bits.telemetry_controller_initiated_data_available);  
  
    printf("  Composite Temperature: %d Kelvins\n", chds->composite_temperature);  
    printf("  Percentage Used: %d%%\n", chds->percentage_used);  
    printf("  Available Spare: %d%%\n", chds->available_spare);  
      
    printf("  Critical Warning: 0x%02X\n", chds->critical_warning.raw);  
    printf("    Spare Threshold: %d\n", chds->critical_warning.bits.spare_threshold);  
    printf("    Temperature Above or Under Threshold: %d\n", chds->critical_warning.bits.temperature_above_or_under_threshold);  
    printf("    Reliability Degraded: %d\n", chds->critical_warning.bits.reliability_degraded);  
    printf("    Read Only: %d\n", chds->critical_warning.bits.read_only);  
    printf("    Volatile Memory Backup Failed: %d\n", chds->critical_warning.bits.volatile_memory_backup_failed);  
    printf("    Persistent Memory Region Error: %d\n", chds->critical_warning.bits.persistent_memory_region_error);  
  
	printf("  Controller Health Status Changed:\n");
	printf("    Ready (RDY): %d\n", chds->flags.bits.ready);
	printf("    Controller Fatal Status (CFS): %d\n", chds->flags.bits.controller_fatal_status);
	printf("    Shutdown Status (SHST): %d\n", chds->flags.bits.shutdown_status);
	printf("    NVM Subsystem Reset Occurred (NSSRO): %d\n", chds->flags.bits.nvm_subsystem_reset_occurred);
	printf("    Controller Enable Change Occurred (CECO): %d\n", chds->flags.bits.controller_enable_change_occurred);
	printf("    Namespace Attribute Changed (NAC): %d\n", chds->flags.bits.namespace_attribute_changed);
	printf("    Firmware Activated (FA): %d\n", chds->flags.bits.firmware_activated);
	printf("    Controller Status Change (CSTS): %d\n", chds->flags.bits.controller_status_change);
	printf("    Composite Temperature Change (CTEMP): %d\n", chds->flags.bits.composite_temperature_change);
	printf("    Percentage Used (PDLU): %d\n", chds->flags.bits.percentage_used);
	printf("    Available Spare (SPARE): %d\n", chds->flags.bits.available_spare);
	printf("    Critical Warning (CWARN): %d\n", chds->flags.bits.critical_warning);
	printf("    Telemetry Controller-Initiated Data Available (TCIDA): %d\n", chds->flags.bits.telemetry_controller_initiated_data_available);
}

static int nvme_amd_reset(struct nvme_dev *dev, __u8 rst, __u8 csi)
{
	struct nvme_mi_mi_req_hdr mi_req = {0};
	struct {
		struct nvme_mi_mi_resp_hdr mi_hdr;
		char buffer[4096];
	} resp = { 0 };

	size_t len = 4096;
	int rc = 0;

	// Opcode for Reset command (0x07 based on enum nvme_mi_command)
	mi_req.opcode = 0x07;
	
	// NVMe Management Dword 0 contains RST field (bits 0-1)
	mi_req.cdw0 = (uint32_t)(rst & 0x03);
	mi_req.cdw0 = mi_req.cdw0 << 24;
	
	printf("Sending Reset command with RST=%d, CSI=%d\n", rst, csi);
	printf("cdw0 field value is: %08x\n", mi_req.cdw0);

	rc = nvme_mi_mi_xfer(dev->mi.ep, &mi_req, 0, &resp.mi_hdr, &len, csi);
	assert(rc == 0);

	printf("\nSTATUS:%d\n", resp.mi_hdr.status);
	
	if (resp.mi_hdr.status) {
		int status = parse_mi_resp_status(resp.mi_hdr.status, (uint8_t *)resp.buffer);
		if (status != 1) return -1;
	} else {
		printf("Reset command completed successfully\n");
	}
	
	return 0;
}

static int reset(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc = (
		"Send a Reset NVMe-MI command to reset the NVM subsystem, controller, or port.\n"
		"Return results.\n");

	const char *rst = "Reset Type: 0=NVM Subsystem Reset";
	const char *csi = "Command slot identifier";

	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int err = 0;

	struct reset_config {
		__u8 rst;
		__u8 csi;
	};

	struct reset_config cfg = {
		.rst = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		OPT_UINT("rst", 'r', &cfg.rst, rst),
		OPT_UINT("csi", 'c', &cfg.csi, csi),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	if (cfg.rst > 0) {
		nvme_show_error("Invalid reset type. Must be 0 (NVM Subsystem Reset)");
		return -EINVAL;
	}

	gettimeofday(&start_time, NULL);
	printf("reset csi %d\n", cfg.csi);
	err = nvme_amd_reset(dev, cfg.rst, cfg.csi);
	gettimeofday(&end_time, NULL);

	return err;
}

static int nvme_amd_shutdown(struct nvme_dev *dev, __u8 sd, __u8 csi)
{
	struct nvme_mi_mi_req_hdr mi_req = {0};
	struct {
		struct nvme_mi_mi_resp_hdr mi_hdr;
		char buffer[4096];
	} resp = { 0 };

	size_t len = 4096;
	int rc = 0;

	// Opcode for Shutdown command (0x0C based on enum nvme_mi_command)
	mi_req.opcode = 0x0C;
	
	// NVMe Management Dword 0 contains SD field (bits 0-1)
	mi_req.cdw0 = (uint32_t)(sd & 0xFF);
	mi_req.cdw0 = mi_req.cdw0 << 24;
	
	printf("Sending Shutdown command with SD=%d, CSI=%d\n", sd, csi);
	printf("cdw0 field value is: %08x\n", mi_req.cdw0);

	rc = nvme_mi_mi_xfer(dev->mi.ep, &mi_req, 0, &resp.mi_hdr, &len, csi);
	assert(rc == 0);

	printf("\nSTATUS:%d\n", resp.mi_hdr.status);
	
	if (resp.mi_hdr.status) {
		int status = parse_mi_resp_status(resp.mi_hdr.status, (uint8_t *)resp.buffer);
		if (status != 1) return -1;
	} else {
		printf("Shutdown command completed successfully\n");
	}
	
	return 0;
}

static int amd_shutdown(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc = (
		"Send a Shutdown NVMe-MI command to prepare the NVM subsystem or controller for shutdown.\n"
		"Return results.\n");

	const char *sd = "Shutdown Type: 0=Normal Shutdown, 1=Abrupt Shutdown";
	const char *csi = "Command slot identifier";

	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int err = 0;

	struct shutdown_config {
		__u8 sd;
		__u8 csi;
	};

	struct shutdown_config cfg = {
		.sd = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		OPT_UINT("sd", 's', &cfg.sd, sd),
		OPT_UINT("csi", 'c', &cfg.csi, csi),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	if (cfg.sd > 1) {
		nvme_show_error("Invalid shutdown type. Must be 0 (Normal Shutdown) or 1 (Abrupt Shutdown)");
		return -EINVAL;
	}

	gettimeofday(&start_time, NULL);
	printf("shutdown csi %d\n", cfg.csi);
	err = nvme_amd_shutdown(dev, cfg.sd, cfg.csi);
	gettimeofday(&end_time, NULL);

	return err;
}

static int ctrlr_hlth_stat_poll(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc =(
		"Send a Controller Health Status Poll command determine changes in health status\n");

	const char *starting_controller_id = "Starting Controller ID (SCTCTL)";
	const char *maximum_response_entries = "Maximum Response Entries (MAXRENT)";
	const char *include_pci_functions = "Include PCI Functions (INCF)";
	const char *include_sr_iov_physical_functions = "Include SR-IOV Physical Functions (INCPFI)";
	const char *include_sr_iov_virtual_functions = "Include SR-IOV Virtual Functions (INCVI)";
	const char *report_all = "Report All (ALL)";
	const char *controller_status_changes = "Controller Status Changes (CSTS)";
	const char *composite_temperature_changes = "Composite Temperature Changes (CTEMP)";
	const char *percentage_used = "Percentage Used (PDLU)";
	const char *available_spare = "Available Spare (SPARE)";  
	const char *critical_warning = "Critical Warning (CWARN)";
	const char *clear_changed_flags = "Clear Changed Flags (CCF)";
    
	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int status = 0, err = 0;
	uint8_t cid = 2, csi = 0;
	size_t len = 4096;

	struct controller_health_status_poll_dwd0 cfg0= {
		.starting_controller_id = 0,
		.maximum_response_entries = 0,
		.include_pci_functions = 0,
		.include_sr_iov_physical_functions = 0,
		.include_sr_iov_virtual_functions = 0,
		.report_all = 0,
	};

	struct controller_health_status_poll_dwd1 cfg1 = {
    		.controller_status_changes = 0,
	    	.composite_temperature_changes = 0,
	    	.percentage_used = 0,
	    	.available_spare = 0,
	    	.critical_warning = 0,
	    	.reserved = 0,
	    	.clear_changed_flags = 0,
	};
	
	struct {
		struct nvme_mi_mi_resp_hdr resp_hdr;
		uint8_t resp_buf[4096];
	} resp = { 0 };

	struct nvme_mi_mi_req_hdr req_hdr = {0};

	OPT_ARGS(opts) = {
	  	OPT_UINT("sctctl", 's', &cfg0.starting_controller_id, starting_controller_id),
		OPT_UINT("maxrent", 'm', &cfg0.maximum_response_entries, maximum_response_entries),
		OPT_UINT("incf", 'f', &cfg0.include_pci_functions, include_pci_functions),
		OPT_UINT("incpfi", 'n', &cfg0.include_sr_iov_physical_functions, include_sr_iov_physical_functions),
		OPT_UINT("incvi", 'i', &cfg0.include_sr_iov_virtual_functions, include_sr_iov_virtual_functions),
		OPT_UINT("all", 'a', &cfg0.report_all, report_all),
		OPT_UINT("csts", 'c', &cfg1.controller_status_changes, controller_status_changes),
		OPT_UINT("ctemp", 't', &cfg1.composite_temperature_changes, composite_temperature_changes),
		OPT_UINT("pdlu", 'p', &cfg1.percentage_used, percentage_used),
		OPT_UINT("spare", 'e', &cfg1.available_spare, available_spare),
		OPT_UINT("cwarn", 'w', &cfg1.critical_warning, critical_warning),
		OPT_UINT("ccf", 'z', &cfg1.clear_changed_flags, clear_changed_flags),
	  	OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;
	uint32_t raw = 0, raw1 = 0;
	raw |= ((uint32_t)cfg0.starting_controller_id & 0xFFFF);           // bits 0-15
	raw |= ((uint32_t)cfg0.maximum_response_entries & 0xFF) << 16;     // bits 16-23
	raw |= ((uint32_t)cfg0.include_pci_functions & 0x1) << 24;         // bit 24
	raw |= ((uint32_t)cfg0.include_sr_iov_physical_functions & 0x1) << 25; // bit 25
	raw |= ((uint32_t)cfg0.include_sr_iov_virtual_functions & 0x1) << 26;  // bit 26
	raw |= ((uint32_t)cfg0.report_all & 0x1) << 31;                    // bit 31
									   //

	raw1 |= (cfg1.controller_status_changes & 0x1);           // bit 0
	raw1 |= (cfg1.composite_temperature_changes & 0x1) << 1;  // bit 1
	raw1 |= (cfg1.percentage_used & 0x1) << 2;                // bit 2
	raw1 |= (cfg1.available_spare & 0x1) << 3;                // bit 3
	raw1 |= (cfg1.critical_warning & 0x1) << 4;               // bit 4
	// bits 5-30 are reserved, leave as 0 or set if needed
	raw1 |= (cfg1.clear_changed_flags & 0x1) << 31;           // bit 31
								  //
	gettimeofday(&start_time, NULL);
	
	//Opcode for Controller Health Status Poll
	req_hdr.opcode = 2;
	req_hdr.cdw0 = raw;
	req_hdr.cdw1 = raw1;
	
	int rc = 0;	
	//Send the message 
	rc = nvme_mi_mi_xfer(dev->mi.ep, &req_hdr, 0, &resp.resp_hdr, &len, csi);
	assert(rc == 0);

	if (resp.resp_hdr.status) {
		int status = parse_mi_resp_status(resp.resp_hdr.status, resp.resp_hdr.nmresp);
		if (status != 1) return -1;
	} else {
		//Number of entries in the response buffer
		uint8_t num_entries = resp.resp_hdr.nmresp[2];
		printf("Number of entries in the response buffer (RENT): %d\n", num_entries);

		// Map resp.resp_buf to controller_health_data_structure_t
		printf("SIZE of CH Data Structure %d\n", sizeof(controller_health_data_structure_t));
		controller_health_data_structure_t *chds = (controller_health_data_structure_t *)resp.resp_buf;

		for (int i = 0; i < num_entries; i++) {
			print_controller_health_data(chds);
			chds = (controller_health_data_structure_t *)((uint8_t *)chds + 16);
		}
	}

	gettimeofday(&end_time, NULL);

	return err;
}

/**
 * Read NVMe-MI Data Structure command interface
 * Implements the command-line interface for the Read NVMe-MI Data Structure command
 * as defined in Section 5.7 of the NVM Express Management Interface Specification
 */
static int read_nvme_mi_data_structure(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc = (
		"Submit a Read NVMe-MI Data Structure command to retrieve information about the "
		"NVM Subsystem, Management Endpoint, or NVMe Controllers. "
		"Different data structure types provide different types of information."
	);

	const char *dtyp = "Data Structure Type (required): "
	                   "0=NVM Subsystem Info, 1=Port Info, 2=Controller List, "
	                   "3=Controller Info, 4=Optional Command List, 5=Mgmt EP Buffer Cmd Support List";
	const char *portid = "Port Identifier (used for DTYP 1,5)";
	const char *ctrlid = "Controller Identifier (used for DTYP 2,3,4)";
	const char *iocsi = "I/O Command Set Identifier (used for DTYP 4,5)";
	const char *csi = "Command slot identifier";

	_cleanup_nvme_dev_ struct nvme_dev *dev = NULL;
	struct timeval start_time, end_time;
	int err = 0;

	struct config {
		__u8 dtyp;
		__u8 portid;
		__u16 ctrlid;
		__u8 iocsi;
		__u8 csi;
	};

	struct config cfg = {
		.dtyp = 0,
		.portid = 0,
		.ctrlid = 0,
		.iocsi = 0,
		.csi = 0,
	};

	OPT_ARGS(opts) = {
		OPT_UINT("dtyp", 'd', &cfg.dtyp, dtyp),
		OPT_UINT("portid", 'p', &cfg.portid, portid),
		OPT_UINT("ctrlid", 'c', &cfg.ctrlid, ctrlid),
		OPT_UINT("iocsi", 'i', &cfg.iocsi, iocsi),
		OPT_UINT("csi", 's', &cfg.csi, csi),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		return err;

	/* Validate Data Structure Type */
	if (cfg.dtyp > NVME_MI_DS_MGMT_EP_BUFFER_CMD_SUPPORT_LIST) {
		printf("ERROR: Invalid Data Structure Type (DTYP): %u. Valid range: 0-5\n", cfg.dtyp);
		return -EINVAL;
	}

	gettimeofday(&start_time, NULL);

	err = nvme_amd_read_data_structure(dev, cfg.dtyp, cfg.portid, cfg.ctrlid, cfg.iocsi, cfg.csi);

	gettimeofday(&end_time, NULL);

	if (err == 0) {
		double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
		                     (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
		printf("\nCommand completed successfully in %.3f seconds\n", elapsed_time);
	}

	return err;
}
