#ifndef _NXP_HDCP_H
#define _NXP_HDCP_H

#include <linux/interrupt.h>

#define AN_SIZE                     8
#define AKSV_SIZE                   5
#define BKSV_SIZE                   5
#define MAX_KEY_SIZE                16

struct work_struct;
struct workqueue_struct;
struct i2c_client;
struct mutex;
struct nxp_hdmi;
struct nxp_v4l2_i2c_board_info;

enum hdcp_event {
	HDCP_EVENT_STOP			= 0,
	HDCP_EVENT_START		= 1 << 0,
	HDCP_EVENT_READ_BKSV_START	= 1 << 1,
	HDCP_EVENT_WRITE_AKSV_START	= 1 << 2,
	HDCP_EVENT_CHECK_RI_START	= 1 << 3,
	HDCP_EVENT_SECOND_AUTH_START	= 1 << 4,
};

static inline char *hdcp_event_to_str(u32 event)
{
	static char event_str[256] = {0, };

	memset(event_str, 0, 256);

	if (event & HDCP_EVENT_START)
		sprintf(event_str, "%s", "HDCP_EVENT_START");
	if (event & HDCP_EVENT_READ_BKSV_START)
		sprintf(event_str, "%s|%s", event_str,
			"HDCP_EVENT_READ_BKSV_START");
	if (event & HDCP_EVENT_WRITE_AKSV_START)
		sprintf(event_str, "%s|%s", event_str,
			"HDCP_EVENT_WRITE_AKSV_START");
	if (event & HDCP_EVENT_CHECK_RI_START)
		sprintf(event_str, "%s|%s", event_str,
			"HDCP_EVENT_CHECK_RI_START");
	if (event & HDCP_EVENT_SECOND_AUTH_START)
		sprintf(event_str, "%s|%s", event_str,
			"HDCP_EVENT_SECOND_AUTH_START");

	return event_str;
}

enum hdcp_auth_state {
	NOT_AUTHENTICATED,
	RECEIVER_READ_READY,
	BCAPS_READ_DONE,
	BKSV_READ_DONE,
	AN_WRITE_DONE,
	AKSV_WRITE_DONE,
	FIRST_AUTHENTICATION_DONE,
	SECOND_AUTHENTICATION_RDY,
	SECOND_AUTHENTICATION_DONE,
};

static inline char *hdcp_auth_state_to_str(u32 state)
{
	static char state_str[64] = {0, };

	memset(state_str, 0, 64);
	switch (state) {
	case NOT_AUTHENTICATED:
		sprintf(state_str, "[%s]", "NOT_AUTHENTICATED");
		break;
	case RECEIVER_READ_READY:
		sprintf(state_str, "[%s]", "RECEIVER_READ_READY");
		break;
	case BCAPS_READ_DONE:
		sprintf(state_str, "[%s]", "BCAPS_READ_DONE");
		break;
	case BKSV_READ_DONE:
		sprintf(state_str, "[%s]", "BKSV_READ_DONE");
		break;
	case AN_WRITE_DONE:
		sprintf(state_str, "[%s]", "AN_WRITE_DONE");
		break;
	case AKSV_WRITE_DONE:
		sprintf(state_str, "[%s]", "AKSV_WRITE_DONE");
		break;
	case FIRST_AUTHENTICATION_DONE:
		sprintf(state_str, "[%s]", "FIRST_AUTHENTICATION_DONE");
		break;
	case SECOND_AUTHENTICATION_RDY:
		sprintf(state_str, "[%s]", "SECOND_AUTHENTICATION_RDY");
		break;
	case SECOND_AUTHENTICATION_DONE:
		sprintf(state_str, "[%s]", "SECOND_AUTHENTICATION_DONE");
		break;
	}

	return state_str;
}

enum {
	ERROR_RI_INVAL = 1,
	ERROR_RI_TIMEOUT = 2,
};

struct nxp_hdcp {
	bool is_repeater;
	bool is_start;

	u32 error;

	enum hdcp_event event;
	enum hdcp_auth_state auth_state;

	struct work_struct work;
	struct workqueue_struct *wq;
	spinlock_t lock;
	irqreturn_t (*irq_handler)(struct nxp_hdcp *);
	int (*prepare)(struct nxp_hdcp *);
	int (*start)(struct nxp_hdcp *);
	int (*stop)(struct nxp_hdcp *);
	int (*suspend)(struct nxp_hdcp *);
	int (*resume)(struct nxp_hdcp *);

	int bus_id; /* i2c adapter id */
	struct i2c_client *client;

	struct mutex mutex;

	/*
	 * asynchronous start worker
	 */
	struct work_struct start_work;

	/*
	 * bksv value can be reused
	 */
	u8 bksv[BKSV_SIZE];
};

/**
 * public api
 */
int  nxp_hdcp_init(struct nxp_hdcp *, struct nxp_v4l2_i2c_board_info *);
void nxp_hdcp_cleanup(struct nxp_hdcp *);

#endif
