#ifndef __SMARTA_MCUCPU_H_
#define __SMARTA_MCUCPU_H_

extern int smarta_mcucpu_send_suspend_started(void);
extern void smarta_mcucpu_send_suspend_done(void);
extern void smarta_mcucpu_send_wakeup_done(void);
extern void smarta_mcucpu_send_dc_state_req(void);
extern void smarta_mcucpu_send_power_off(void);
extern void smarta_mcucpu_send_dummy_key_event(void);

#endif //__SMARTA_MCUCPU_H_