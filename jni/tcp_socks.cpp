#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "platform.h"

#include "module.h"
#include "slotwait.h"
#include "slotsock.h"

#include "tcpusr.h"
#include "tcp_socks.h"

#define TF_CONNECT    1
#define TF_CONNECTING 2
#define TF_EOF0       4
#define TF_EOF1       8
#define TF_SHUT0      16 
#define TF_SHUT1      32
#define TF_SOCKS      64
#define TF_AUTHED     128
#define TF_VERSION4   256

class tcp_socks {
   	public:
		tcp_socks(int tcpfd);
		~tcp_socks();

	public:
		int run(void);
		static void tc_callback(void *context);

	private:
		int m_file;
		int m_flags;

	private:
		struct waitcb m_rwait;
		struct waitcb m_wwait;
		struct sockcb *m_sockcbp;

	private:
		int m_woff;
		int m_wlen;
		char m_wbuf[8192];

	private:
		int m_roff;
		int m_rlen;
		char m_rbuf[8192];

	private:
		int m_prfd;
		struct waitcb r_evt_peer;
		struct waitcb w_evt_peer;
		struct sockcb *m_peer;
};

tcp_socks::tcp_socks(int tcpfd)
	:m_flags(0)
{
	m_prfd = tcpfd;
	m_file = socket(AF_INET, SOCK_STREAM, 0);
	assert(m_file != -1);
	
	m_roff = m_rlen = 0;
	m_woff = m_wlen = 0;
	m_peer = sock_attach(tcpfd);
	m_sockcbp = sock_attach(m_file);
	waitcb_init(&m_rwait, tc_callback, this);
	waitcb_init(&m_wwait, tc_callback, this);
	waitcb_init(&r_evt_peer, tc_callback, this);
	waitcb_init(&w_evt_peer, tc_callback, this);
}

tcp_socks::~tcp_socks()
{
	waitcb_clean(&m_rwait);
	waitcb_clean(&m_wwait);
	waitcb_clean(&r_evt_peer);
	waitcb_clean(&w_evt_peer);

	fprintf(stderr, "tcp_socks::~tcp_socks\n");
	sock_detach(m_sockcbp);
	closesocket(m_file);
	sock_detach(m_peer);
	closesocket(m_prfd);
}

int tcp_socks::run(void)
{
	int len = 0;
	int error = -1;
	in_addr in_addr1;
	u_short in_port1;	
	socklen_t addr_len1;
	struct sockaddr_in addr_in1;

	while ((TF_SOCKS & m_flags) == 0) {
		int need_restart = 0;

		if (waitcb_completed(&r_evt_peer) && m_wlen < (int)sizeof(m_wbuf)) {
			len = recv(m_prfd, m_wbuf + m_wlen, sizeof(m_wbuf) - m_wlen, 0);
			if (len > 0)
				m_wlen += len;
			else if (len == 0)
				m_flags |= TF_EOF0;
			else if (WSAGetLastError() != WSAEWOULDBLOCK)
				return 0;
			waitcb_clear(&r_evt_peer);
		}

		if (waitcb_completed(&w_evt_peer) && m_roff < m_rlen) {
			do {
				len = send(m_prfd, m_rbuf + m_roff, m_rlen - m_roff, 0);
				if (len > 0)
					m_roff += len;
				else if (WSAGetLastError() == WSAEWOULDBLOCK)
					waitcb_clear(&w_evt_peer);
				else
					return 0;
			} while (len > 0 && m_roff < m_rlen);
		}

		if ((TF_AUTHED & m_flags) == 0 && m_wlen > 0) {
			if (m_wbuf[0] != 0x04 && m_wbuf[0] != 0x05) {
				/* bad protocol handshake */
				return 0;
			}
		}

		if ((TF_AUTHED & m_flags) == 0 &&
			m_wlen > 0 && m_wbuf[0] == 0x04) {
			if (m_wlen > 2 && m_wbuf[1] != 0x01) {
				/* only 0x1 is supported */
				return 0;
			}

			char *pfin = m_wlen < 9? 0: (char *)memchr(m_wbuf + 8, 0, m_wlen - 8);
			if (m_wlen < 9 || pfin == NULL) {
				if (m_wlen >= (int)sizeof(m_wbuf)) {
					/* buffer is full */
					return 0;
				}
			} else {
				memcpy(&in_addr1, &m_wbuf[4], sizeof(in_addr1));
				memcpy(&in_port1, &m_wbuf[2], sizeof(in_port1));
				m_wlen -= (++pfin - m_wbuf);

				addr_in1.sin_family = AF_INET;
				addr_in1.sin_port   = in_port1;
				addr_in1.sin_addr   = in_addr1;
				error = connect(m_file, (sockaddr *)&addr_in1, sizeof(addr_in1));
				if (error == -1 && WSAGetLastError() != WSAEINPROGRESS)
					return 0;

				m_flags |= TF_AUTHED;
				m_flags |= TF_VERSION4;
				m_flags |= TF_CONNECT;
				m_flags |= (error? TF_CONNECTING: 0);
				waitcb_clear(&m_rwait);
			}
		}

		int nmethod;
		if ((m_flags & TF_AUTHED) == 0 &&
			m_wlen > 0 && m_wbuf[0] == 0x05) {

			nmethod = (m_wbuf[1] & 0xFF);
			if (m_wlen >= 2 && (nmethod + 2) >= int(m_wlen)) {
				if (memchr(m_wbuf + 2, 0x0, nmethod) != NULL) {

					m_wbuf[1] = 0;
					memcpy(m_rbuf + m_rlen, m_wbuf, 2);
					m_rlen += 2;

					m_wlen -= (2 + nmethod);
					memmove(m_wbuf, &m_wbuf[2 + nmethod], m_wlen);

					m_flags |= TF_AUTHED;
					need_restart = 1;
				}
			}
		}

		u_char pro_seq[] = {0x05, 0x01, 0x00, 0x01};
		u_char pro_seq_nam[] = {0x05, 0x01, 0x00, 0x03};
		int test_flags = TF_AUTHED| TF_CONNECTING;
		if ((test_flags & m_flags) == TF_AUTHED) {
			if (m_wlen >= 4 && !memcmp(pro_seq_nam, m_wbuf, 4)) {
				int namlen;
				char hostname[128];
				struct hostent *host;
			   	namlen = (m_wbuf[4] & 0xFF);
				if (m_wlen >= namlen + 7) {
					memcpy(hostname, m_wbuf + 5, namlen);
					memcpy(&in_port1, &m_wbuf[5 + namlen], sizeof(in_port1));
					hostname[namlen] = 0;
				   	m_wlen -= (7 + namlen);
					memmove(m_wbuf, m_wbuf + 7 + namlen, m_wlen);

					host = gethostbyname(hostname);
					if (host == NULL) {
						return 0;
					}
				   
					addr_in1.sin_family = AF_INET;
				   	addr_in1.sin_port   = in_port1;
				   	memcpy(&addr_in1.sin_addr, host->h_addr, 4);
				   	error = connect(m_file, (sockaddr *)&addr_in1, sizeof(addr_in1));
				   	if (error == -1 && WSAGetLastError() != WSAEINPROGRESS)
					   	return 0;

					m_flags |= TF_CONNECT;
				   	m_flags |= (error? TF_CONNECTING: 0);
				   	need_restart = 1;
					waitcb_clear(&m_wwait);
				}
			} else if (m_wlen >= 10 && !memcmp(pro_seq, m_wbuf, 4)) {
				m_wlen -= 10;
				memmove(m_wbuf, &m_wbuf[10], m_wlen);
				memcpy(&in_addr1, &m_wbuf[4], sizeof(in_addr1));
				memcpy(&in_port1, &m_wbuf[8], sizeof(in_port1));

				addr_in1.sin_family = AF_INET;
				addr_in1.sin_port   = in_port1;
				addr_in1.sin_addr   = in_addr1;
				error = connect(m_file, (sockaddr *)&addr_in1, sizeof(addr_in1));
				if (error == -1 && WSAGetLastError() != WSAEINPROGRESS)
					return 0;

				m_flags |= TF_CONNECT;
				m_flags |= (error? TF_CONNECTING: 0);
				need_restart = 1;
				waitcb_clear(&m_wwait);
			}
		}

		u_char resp[] = {
			0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		u_char resp_v4[] = {
			0x00, 0x5A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
		};

		test_flags = TF_CONNECTING| TF_CONNECT;
		if ((m_flags & test_flags) == TF_CONNECT ||
			(waitcb_completed(&m_wwait) && (m_flags & TF_CONNECTING))) {

			error = getsockname(m_file, (sockaddr *)&addr_in1, &addr_len1);
			if (error == 0) {
				in_port1 = addr_in1.sin_port;
				memcpy(&resp[8], &in_port1, sizeof(in_port1));

				in_addr1 = addr_in1.sin_addr;
				memcpy(&resp[4], &in_addr1, sizeof(in_addr1));
			}

			if (m_flags & TF_VERSION4) {
				memcpy(m_rbuf + m_rlen, resp_v4, 8);
				m_rlen += 8;
			} else {
				memcpy(m_rbuf + m_rlen, resp, 10);
				m_rlen += 10;
			}

			m_flags |= TF_SOCKS;
			break;
		}

		if (need_restart == 0) {
			if (m_flags & TF_EOF0) {
				return 0;
			}

			if (m_wlen < (int)sizeof(m_wbuf)) {
				sock_read_wait(m_peer, &r_evt_peer);
			}

			if (m_roff < m_rlen) {
				sock_write_wait(m_peer, &w_evt_peer);
			}

			if (m_flags & TF_CONNECTING) {
				sock_write_wait(m_sockcbp, &m_wwait);
			}

			return 1;
		}
	}

reread:
	if (waitcb_completed(&m_rwait) && m_rlen < (int)sizeof(m_rbuf)) {
		len = recv(m_file, m_rbuf + m_rlen, sizeof(m_rbuf) - m_rlen, 0);
		if (len > 0)
		   	m_rlen += len;
		else if (len == 0)
			m_flags |= TF_EOF1;
		else if (WSAGetLastError() != WSAEWOULDBLOCK)
			return 0;
		waitcb_clear(&m_rwait);
	}

	if (waitcb_completed(&r_evt_peer) && m_wlen < (int)sizeof(m_wbuf)) {
		len = recv(m_prfd, m_wbuf + m_wlen, sizeof(m_wbuf) - m_wlen, 0);
		if (len > 0)
		   	m_wlen += len;
		else if (len == 0)
			m_flags |= TF_EOF0;
		else if (WSAGetLastError() != WSAEWOULDBLOCK)
			return 0;
		waitcb_clear(&r_evt_peer);
	}

	if (waitcb_completed(&m_wwait) && m_woff < m_wlen) {
		do {
			len = send(m_file, m_wbuf + m_woff, m_wlen - m_woff, 0);
			if (len > 0)
				m_woff += len;
			else if (WSAGetLastError() == WSAEWOULDBLOCK)
				waitcb_clear(&m_wwait);
			else
				return 0;
		} while (len > 0 && m_woff < m_wlen);
	}

	if (waitcb_completed(&w_evt_peer) && m_roff < m_rlen) {
		do {
			len = send(m_prfd, m_rbuf + m_roff, m_rlen - m_roff, 0);
			if (len > 0)
				m_roff += len;
			else if (WSAGetLastError() == WSAEWOULDBLOCK)
				waitcb_clear(&w_evt_peer);
			else
				return 0;
		} while (len > 0 && m_roff < m_rlen);
	}

	error = 0;

	if (m_roff >= m_rlen) {
		int test_flags = (TF_EOF1 | TF_SHUT1);
	   
		m_roff = m_rlen = 0;
		if ((m_flags & test_flags) == TF_EOF1) {
			test_flags |= TF_SHUT1;
			shutdown(m_prfd, SD_BOTH);
		} else {
			if (waitcb_completed(&m_rwait))
				goto reread;
		}
	}

	if (m_woff >= m_wlen) {
		int test_flags = (TF_EOF0 | TF_SHUT0);
		if ((m_flags & test_flags) == TF_EOF0) {
			shutdown(m_file, SD_BOTH);
			test_flags |= TF_SHUT0;
		}
		m_woff = m_wlen = 0;
	}

	if (m_roff < m_rlen) {
		sock_write_wait(m_peer, &w_evt_peer);
		error = 1;
	}

	if (m_woff < m_wlen) {
	   	sock_write_wait(m_sockcbp, &m_wwait);
		error = 1;
	}

	if (m_rlen < (int)sizeof(m_rbuf) &&
			(m_flags & TF_EOF1) == 0) {
	   	sock_read_wait(m_sockcbp, &m_rwait);
		error = 1;
	}

	if (m_wlen < (int)sizeof(m_wbuf) &&
			(m_flags & TF_EOF0) == 0) {
		sock_read_wait(m_peer, &r_evt_peer);
		error = 1;
	}

	return error;
}

void tcp_socks::tc_callback(void *context)
{
	tcp_socks *chan;
	chan = (tcp_socks *)context;

	if (chan->run() == 0) {
		delete chan;
		return;
	}
   
	return;
}

void new_tcp_socks(int tcpfd)
{
	/* int error; */
	tcp_socks *chan;
   	chan = new tcp_socks(tcpfd);

	if (chan == NULL) {
		closesocket(tcpfd);
		return;
	}

	tcp_socks::tc_callback(chan);
	return;
}

