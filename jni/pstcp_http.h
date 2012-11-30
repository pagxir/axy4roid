#ifndef _PSTCP_HTTP_H_
#define _PSTCP_HTTP_H_
struct tcpcb;
void new_pstcp_http(struct tcpcb * tp, const char * path);
#endif

