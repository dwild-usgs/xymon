/*----------------------------------------------------------------------------*/
/* Xymon monitor network test tool.                                           */
/*                                                                            */
/* Copyright (C) 2004-2011 Henrik Storner <henrik@hswn.dk>                    */
/*                                                                            */
/* This program is released under the GNU General Public License (GPL),       */
/* version 2. See the file "COPYING" for details.                             */
/*                                                                            */
/*----------------------------------------------------------------------------*/

#ifndef __DNSTALK_H__
#define __DNSTALK_H__

extern void build_dns_request(myconn_t *rec);
extern int start_dns_query(myconn_t *rec, char *targetserver);
extern int dns_add_active_fds(int *maxfd, fd_set *fdread, fd_set *fdwrite);
extern void dns_process_active(fd_set *fdread, fd_set *fdwrite);
extern void dns_trimactive(void);

#endif
