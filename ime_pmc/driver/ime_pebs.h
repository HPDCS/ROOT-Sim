void pebs_init(void *arg);

void pebs_exit(void *arg);

void write_buffer(void);

void prinf_pebs(void);

int init_pebs_struct(void);

void exit_pebs_struct(void);

ssize_t ime_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);