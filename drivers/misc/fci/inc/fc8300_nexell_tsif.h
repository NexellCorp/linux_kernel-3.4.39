#ifndef __FC8300_NEXELL_TSIF_H__
#define	__FC8300_NEXELL_TSIF_H__

int tsif_get_channel_num(void);
int tsif_init(u8);
int tsif_deinit(u8);
int tsif_start(u8);
void tsif_stop(u8);
int tsif_read(u8, void *, int);
int tsif_alloc_buf(u8);

extern int ts_initialize(struct ts_config_descr *, struct ts_param_descr *,
				struct ts_buf_init_info *);
extern int ts_deinitialize(U8);
extern int ts_start(struct ts_op_mode *);
extern void ts_stop(U8);
extern int ts_read(struct ts_param_descr *);
extern int ts_init_buf(struct ts_buf_init_info *);

#endif
