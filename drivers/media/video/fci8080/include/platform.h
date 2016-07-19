#ifndef __PLATFORMS_H__
#define __PLATFORMS_H__

#ifdef __cplusplus
extern "C" {
#endif

void platform_hw_setting(void);
void platform_hw_init(u32 power_pin, u32 reset_pin);
void platform_hw_deinit(u32 power_pin);
void platform_hw_reset(u32 reset_pin);

#ifdef __cplusplus
}
#endif

#endif
