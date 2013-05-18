#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <wait/module.h>
#include <wait/platform.h>
#include <wait/slotwait.h>
#include <wait/slotsock.h>

#include "pstcp_http.h"

#define TF_CONNECT    1
#define TF_CONNECTING 2
#define TF_EOF0       4
#define TF_EOF1       8
#define TF_RCV_HEADER 16 
#define TF_SHUT1      32

class pstcp_http {
   	public:
		pstcp_http(int fd, const char * path);
		~pstcp_http();

	public:
		int run(void);
		static void tc_callback(void * context);

	private:
		int m_woff;
		int m_wlen;
		char m_wbuf[8192];
		int create_response_stream(const char * path, int range, size_t length, size_t offset);

	private:
		int m_roff;
		int m_rlen;
		char m_rbuf[8192];

	private:
		int m_peer;
		int m_flags;
		FILE * m_file;
		char m_folder[512];
		struct sockcb *m_peercb;
		struct waitcb r_evt_peer;
		struct waitcb w_evt_peer;
};

pstcp_http::pstcp_http(int fd, const char * path)
	: m_flags(0), m_file(NULL)
{
	m_peer = fd;
	m_roff = m_rlen = 0;
	m_woff = m_wlen = 0;
	strncpy(m_folder, path, sizeof(m_folder));

	int len = strlen(m_folder);
	if (len > 0 && (m_folder[len - 1] != '/')) {
		m_folder[len++] = '/';
		m_folder[len++] = 0;
	}
	m_peercb = sock_attach(fd);
	waitcb_init(&r_evt_peer, tc_callback, this);
	waitcb_init(&w_evt_peer, tc_callback, this);
}

pstcp_http::~pstcp_http()
{
	waitcb_clean(&r_evt_peer);
	waitcb_clean(&w_evt_peer);

	sock_detach(m_peercb);
	fprintf(stderr, "pstcp_http::~pstcp_http\n");
	if (m_file != 0)
	   	fclose(m_file);
	close(m_peer);
}

static int get_request_range(const char * request, size_t * plength, size_t * poffset)
{
	const char * opt;

	opt = strstr(request, "\r\nRange:");
	if (opt == NULL)
		return 0;

	opt += sizeof("\r\nRange");
	while (isspace(*opt)) opt++;

	if (strncmp(opt, "bytes=", 6) == 0) {
		sscanf(opt + 6, "%lu", poffset);
		return 1;
	}

	return 0;
}

static char xd(const char * p)
{
	int ch1, ch2, chd;

	chd = 0;
	ch1 = *p++;
	ch2 = *p++;

	if ('0' <= ch1 && ch1 <= '9')
		chd = (ch1 - '0');
	else if ('a' <= ch1 && ch1 <= 'f')
		chd = (ch1 - 'a' + 10);
	else if ('A' <= ch1 && ch1 <= 'F')
		chd = (ch1 - 'A' + 10);

	chd <<= 4;
	
	if ('0' <= ch2 && ch2 <= '9')
		chd |= (ch2 - '0');
	else if ('a' <= ch2 && ch2 <= 'f')
		chd |= (ch2 - 'a' + 10);
	else if ('A' <= ch2 && ch2 <= 'F')
		chd |= (ch2 - 'A' + 10);

	return chd;
}

static char * url_decode(char * url)
{
	char * outp;
	const char * inp;

	inp = outp = url;
	while (*inp) {
		if (*inp == '%' && 
			isxdigit(inp[1]) && isxdigit(inp[2])) {
				*outp++ = xd(++inp);
				inp += 2;
		} else {
			*outp++ = *inp;
			inp++;
		}
	};

	*outp = 0;

	return url;
}

static int get_request_path(const char * request, char * buf, size_t len)
{
	char * end_slash;
	const char * slash = request;

	end_slash = (buf + len - 1);
	while (isspace(*slash))
		slash++;

	while (isalpha(*slash))
		slash++;

	if (!isspace(*slash))
		return 0;

	while (isspace(*slash))
		slash++;

	while (*slash &&
		   	!isspace(*slash)) {
		if (len == 0)
			break;

		if (*slash == '\r')
			break;

		if (*slash == '\n')
			break;

		*buf++ = *slash++;
		len--;
	}

	if (len > 0) {
		*buf = 0;
		return 0;
	}

	*end_slash = 0;
	return 0;
}

char resp404_template[] = {
	"HTTP/1.1 404 Not Found\r\n"
	"Server: nginx/0.7.63\r\n"
	"Date: Mon, 03 Jan 2011 12:15:23 GMT\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: %u\r\n"
	"Connection: close\r\n"
	"\r\n"
};

char resp404_body[] = {
	"<html>\r\n"
	"<head><title>400 Bad Request</title></head>\r\n"
	"<body bgcolor='white'>\r\n"
	"<center><h1>400 Bad Request</h1></center>\r\n"
	"<hr><center>nginx/0.7.63</center>\r\n"
	"</body>\r\n"
    "</html>\r\n"
};

char resp200_template[] = {
	"HTTP/1.1 200 OK\r\n"
	"Date: Mon, 03 Jan 2011 10:54:18 GMT\r\n"
	"Server: BWS/1.0\r\n"
	"Content-Length: %lu\r\n"
	"Content-Type: %s\r\n" /* text/html */
	"Expires: Mon, 03 Jan 2011 10:54:18 GMT\r\n"
	"Connection: Close\r\n"
	"\r\n"
};

char resp206_template[] = {
	"HTTP/1.1 206 Partial Content\r\n"
	"Date: Mon, 03 Jan 2011 10:54:18 GMT\r\n"
	"Server: BWS/1.0\r\n"
	"Content-Range: bytes %u-%u/%u\r\n"
	"Content-Type: %s\r\n"
	"Content-Length: %u\r\n"
	"Expires: Mon, 03 Jan 2011 10:54:18 GMT\r\n"
	"Connection: Close\r\n"
	"\r\n"
};

const char * get_file_type(const char * path)
{
	const char * ext = strrchr(path, '.');

	if (ext == NULL)
	   	return "application/octet-stream";

	if (stricmp(ext, ".swf") == 0)
		return "application/x-shockwave-flash";

	if (stricmp(ext, ".jpg") == 0)
		return "image/jpg";

	if (stricmp(ext, ".png") == 0)
		return "image/png";

	if (stricmp(ext, ".html") == 0)
		return "text/html";

	if (stricmp(ext, ".htm") == 0)
		return "text/html";

	if (stricmp(ext, ".txt") == 0)
		return "text/plain";

	if (stricmp(ext, ".txt") == 0)
		return "text/plain";

	if (stricmp(ext, ".c") == 0)
		return "text/plain";

	if (stricmp(ext, ".cpp") == 0)
		return "text/plain";

	return "application/octet-stream";
}

static long get_file_length(FILE *fp)
{
	struct stat64 st;

	if (fstat64(fileno(fp), &st) == 0)
		return st.st_size;

	return 0;
}

int pstcp_http::create_response_stream(const char * path0, int range, size_t length, size_t offset)
{
	int fldcnt = 0;
	char * pfolder;
	const char * path;
	long file_len = 0;

	path = path0;
	pfolder = m_folder + strlen(m_folder);
	if (*path == 0 || *path != '/')
		path = "/index.html";
	
	while (*++path && fldcnt >= 0) {

		if (pfolder - m_folder >= (int)sizeof(m_folder)) {
			*(pfolder - 1) = 0;
			break;
		}

		if (*path == '\\') {
			*pfolder++ = '#';
			continue;
		}

		if (*path != '/') {
			*pfolder++ = *path;
			continue;
		}

		fldcnt++;
		*pfolder ++ = '/';

		if (pfolder - m_folder > 3 &&
			strncmp(pfolder - 3, "/./", 3) == 0) {
			pfolder -= 3;
			fldcnt--;
		} else if (pfolder - m_folder > 4 &&
			strncmp(pfolder - 4, "/../", 4) == 0) {
			pfolder -= 4;
			fldcnt--;

			while(pfolder > m_folder &&
				*(pfolder - 1) != '/') {
				pfolder--;
			}
			
			fldcnt--;
		}
	}

	m_file = NULL;
	if (fldcnt >= 0) {
		/* WCHAR w_path[512]; */
		m_file = fopen(m_folder, "rb");
	}

	if (m_file == NULL) {
		printf("not found: %s\n", path);
		sprintf(m_rbuf, resp404_template, strlen(resp404_body));
		strncat(m_rbuf, resp404_body, sizeof(m_rbuf));
		m_rlen = strlen(m_rbuf);
		m_flags |= TF_EOF1;
		return 0;
	}

	file_len = get_file_length(m_file);

	switch (range) {
		case 0:
			sprintf(m_rbuf, resp200_template, file_len, get_file_type(path0));
			m_rlen = strlen(m_rbuf);
			break;

		case 1:
			fseek(m_file, offset, SEEK_SET);
			sprintf(m_rbuf, resp206_template,
				offset, file_len - 1, file_len, get_file_type(path0), file_len - offset);
			m_rlen = strlen(m_rbuf);
			break;

		default:
			assert(0);
			break;
	}

	return 0;
}


int pstcp_http::run(void)
{
	int len = 0;
	int error = -1;

	if (waitcb_completed(&r_evt_peer) && m_wlen < (int)sizeof(m_wbuf)) {
		len = read(m_peer, m_wbuf + m_wlen, sizeof(m_wbuf) - m_wlen);
		if (len == -1 || len == 0) {
			m_flags |= TF_EOF0;
			len = 0;
		}
		waitcb_clear(&r_evt_peer);
		m_wlen += len;
	}

	if (waitcb_completed(&w_evt_peer) && m_roff < m_rlen) {
		len = write(m_peer, m_rbuf + m_roff, m_rlen - m_roff);
		if (len == -1)
			return 0;
		waitcb_clear(&w_evt_peer);
		m_roff += len;
	}

	error = 0;

	if (m_wlen > 0) {
		int range;
		char path[512];
	   	size_t offset, length = 0;

		if (m_flags & TF_RCV_HEADER) {
			/* if recv header, then drop any incoming data. */
			m_wlen = 0;
		}

		if ((m_flags & TF_RCV_HEADER) == 0 &&
				(strstr(m_wbuf, "\r\n\r\n") || (m_flags & TF_EOF0))) {
			m_flags |= TF_RCV_HEADER;
			m_wbuf[m_wlen - (m_wlen == sizeof(m_wbuf))] = 0;
			range = get_request_range(m_wbuf, &length, &offset);
			get_request_path(m_wbuf, path, sizeof(path));
			create_response_stream(url_decode(path), range, length, offset);
			m_wlen = 0;
		}
	}

	if (m_roff == m_rlen) {
		int test_flags1 = (TF_EOF1 | TF_SHUT1);
		int test_flags = (TF_EOF1 | TF_RCV_HEADER);
		
		if ((m_flags & test_flags) == TF_RCV_HEADER) {
			m_rlen = fread(m_rbuf, 1, sizeof(m_rbuf), m_file);
			if (m_rlen == 0)
				m_flags |= TF_EOF1;
			m_roff = 0;
		}

		if ((m_flags & test_flags1) == TF_EOF1) {
			test_flags |= TF_SHUT1;
			shutdown(m_peer, SHUT_WR);
		} 
	}

	if (m_roff < m_rlen) {
		sock_write_wait(m_peercb, &w_evt_peer);
		error = 1;
	}

	if (m_wlen < (int)sizeof(m_wbuf) &&
			(m_flags & TF_EOF0) == 0) {
		sock_read_wait(m_peercb, &r_evt_peer);
		error = 1;
	}

	return error;
}

void pstcp_http::tc_callback(void * context)
{
	pstcp_http * chan;
	chan = (pstcp_http *)context;

	if (chan->run() == 0) {
		delete chan;
		return;
	}
   
	return;
}

void new_pstcp_channel(int fd)
{
	pstcp_http * chan;
	const char * path = "/";
   	chan = new pstcp_http(fd, path);

	if (chan == NULL) {
		close(fd);
		return;
	}

	pstcp_http::tc_callback(chan);
	return;
}

void pstcp_channel_forward(u_long addr, u_short port)
{
}
