#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#define HTTP_HEADER				0x01
#define HTTP_DISABLED			0x20
#define CHUNKED_HEADER			0x04
#define HTTP_CHUNKED_TRANSFER	0x08
#define HTTP_CONTENT_COMPLETE	0x10
#define MAX_HTTP_HEAD_LEN		8192

class channel {
	public:
		virtual int blocking(int code) = 0;
		virtual int compact(int force) = 0;
		virtual int close(void) = 0;
		virtual ~channel() = 0;

	public:
		virtual int ack(size_t len) = 0;
		virtual int get(char **ptr) = 0;
		virtual int put(const char *buf, size_t len) = 0;

	public:
		virtual int waiti(struct waitcb *wait) = 0;
		virtual int waito(struct waitcb *wait) = 0;
};

class tcp_channel: public channel {
	public:
		tcp_channel(int fd);
		~tcp_channel();

	public:
		int ack(size_t len);
		int get(char **ptr);
		int put(const char *buf, size_t len);

		int blocking(int code);
		int compact(int force);
		int close(void);

	public:
		int waiti(struct waitcb *wait);
		int waito(struct waitcb *wait);

	private:
		int mfd;
		int mclosed;
		struct sockcb *msockcb;

	protected:
		int moff, mlen;
		char mbuf[MAX_HTTP_HEAD_LEN + 1];
};

class http_raw_channel: public tcp_channel {
	public:
		http_raw_channel(int fd);
		~http_raw_channel();

	public:
		int open_next(void);
		int enable_http(void);
		int update_get(const char *buf, size_t len);

		int compact(int force);
		int ack(size_t len);
		int get(char **ptr);

	private:
		int handle_http_header_item(const char *buf, size_t len);
		int handle_http_chunked_header(const char *buf, size_t len);

	private:
		int t_off;
		int t_len;

	private:
		int mflags;
		int mdata_length;
		int mchunked_length, mcontent_length;
};

class channel_forward: private waitcb {
	public:
		channel_forward(wait_call *call, void *up);
		~channel_forward();

	public:
		void start(channel *src, channel *dst);
		void cancel(void);

	private:
		channel *_src;
		channel *_dst;

	private:
		struct waitcb r_wait;
		struct waitcb w_wait;

	private:
		void callback(void);
		static void cf_callback(void *up);
};

#endif
