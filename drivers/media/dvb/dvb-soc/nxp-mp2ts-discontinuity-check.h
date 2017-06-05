#define MAX_PIDS		24

static struct {
	unsigned short PID;
	unsigned int Counts;
	unsigned int Discontinuities;
	unsigned int Last_CC;
} Stats[MAX_PIDS];
static unsigned int Totals;

extern unsigned int debugFVPid;
extern unsigned int debugFAPid;
extern unsigned int debugOVPid;
extern unsigned int debugOAPid;

void check_ts_reset(void);
void check_ts_print(void);
int check_ts(unsigned char *buf, unsigned int len, unsigned int packet_size);

int file_open(char *path, int flags, int rights);
void file_close(int fd);
int file_read(int fd, unsigned long long offset, unsigned char *data,
		unsigned int size);
int file_write(int fd, unsigned long long offset, unsigned char *data,
		unsigned int size);
int file_sync(int fd);
