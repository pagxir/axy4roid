#ifndef _CHANNEL_H_
#define _CHANNEL_H_
class channel {
	public:
		virtual int close(void) = 0;
		virtual ~channel() = 0;

	public:
		virtual int read(void *buf, size_t len) = 0;
		virtual int write(void *buf, size_t len) = 0;

	public:
		virtual int waiti(struct waitcb *wait) = 0;
		virtual int waito(struct waitcb *wait) = 0;
};

class tcp_channel: public channel {
	public:
		tcp_channel(int fd);
		~tcp_channel();

	public:
		int read(void *buf, size_t len);
		int write(void *buf, size_t len);
		int close(void);

	public:
		int waiti(struct waitcb *wait);
		int waito(struct waitcb *wait);

	private:
		int mfd;
		int mclosed;
		struct sockcb *msockcb;
};

class http_raw_channel: public tcp_channel {
	public:
		http_raw_channel(int fd);
		~http_raw_channel();

	public:
		int read(void *buf, size_t len);
		int write(void *buf, size_t len);

		int open_next(void);
		int close(void);

	public:
		int waiti(struct waitcb *wait);
		int waito(struct waitcb *wait);
};

#endif
