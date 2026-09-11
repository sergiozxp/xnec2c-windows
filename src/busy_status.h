#ifndef BUSY_STATUS_H
#define BUSY_STATUS_H 1

void busy_status_load_begin(void);
void busy_status_load_end(void);
void busy_status_sweep_begin(void);
void busy_status_sweep_end(void);
void busy_status_sweep_end_async(void);

#endif
