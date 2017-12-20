/* OCaml promise library
 * http://www.ocsigen.org/lwt
 * Copyright (C) 2009-2010 Jérémie Dimino
 *               2009 Mauricio Fernandez
 *               2010 Pierre Chambart
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, with linking exceptions;
 * either version 2.1 of the License, or (at your option) any later
 * version. See COPYING file for details.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "lwt_config.h"

#if !defined(LWT_ON_WINDOWS)

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/socketaddr.h>
#include <caml/unixsupport.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "unix_recv_send_utils.h"
/* Convert a caml list of io-vectors into a C array io io-vector
   structures */
void store_iovs(struct iovec *iovs, value iovs_val)
{
    CAMLparam0();
    CAMLlocal2(list, x);
    for (list = iovs_val; Is_block(list); list = Field(list, 1), iovs++) {
        x = Field(list, 0);
        iovs->iov_base = &Byte(String_val(Field(x, 0)), Long_val(Field(x, 1)));
        iovs->iov_len = Long_val(Field(x, 2));
    }
    CAMLreturn0;
}

void bytes_store_iovs(struct iovec *iovs, value iovs_val)
{
    CAMLparam0();
    CAMLlocal2(list, x);
    for (list = iovs_val; Is_block(list); list = Field(list, 1), iovs++) {
        x = Field(list, 0);
        iovs->iov_base =
            (char *)Caml_ba_data_val(Field(x, 0)) + Long_val(Field(x, 1));
        iovs->iov_len = Long_val(Field(x, 2));
    }
    CAMLreturn0;
}

value wrapper_recv_msg(int fd, int n_iovs, struct iovec *iovs)
{
    CAMLparam0();
    CAMLlocal3(list, result, x);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iovs;
    msg.msg_iovlen = n_iovs;
#if defined(HAVE_FD_PASSING)
    msg.msg_controllen = CMSG_SPACE(256 * sizeof(int));
    msg.msg_control = alloca(msg.msg_controllen);
    memset(msg.msg_control, 0, msg.msg_controllen);
#endif

    int ret = recvmsg(fd, &msg, 0);
    if (ret == -1) uerror("recv_msg", Nothing);

    list = Val_int(0);
#if defined(HAVE_FD_PASSING)
    struct cmsghdr *cm;
    for (cm = CMSG_FIRSTHDR(&msg); cm; cm = CMSG_NXTHDR(&msg, cm))
        if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
            int *fds = (int *)CMSG_DATA(cm);
            int nfds = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int i;
            for (i = nfds - 1; i >= 0; i--) {
                x = caml_alloc_tuple(2);
                Store_field(x, 0, Val_int(fds[i]));
                Store_field(x, 1, list);
                list = x;
            };
            break;
        };
#endif

    result = caml_alloc_tuple(2);
    Store_field(result, 0, Val_int(ret));
    Store_field(result, 1, list);
    CAMLreturn(result);
}

value wrapper_send_msg(int fd, int n_iovs, struct iovec *iovs,
                       value val_n_fds, value val_fds)
{
    CAMLparam2(val_n_fds, val_fds);

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iovs;
    msg.msg_iovlen = n_iovs;

    int n_fds = Int_val(val_n_fds);
#if defined(HAVE_FD_PASSING)
    if (n_fds > 0) {
        msg.msg_controllen = CMSG_SPACE(n_fds * sizeof(int));
        msg.msg_control = alloca(msg.msg_controllen);
        memset(msg.msg_control, 0, msg.msg_controllen);

        struct cmsghdr *cm;
        cm = CMSG_FIRSTHDR(&msg);
        cm->cmsg_level = SOL_SOCKET;
        cm->cmsg_type = SCM_RIGHTS;
        cm->cmsg_len = CMSG_LEN(n_fds * sizeof(int));

        int *fds = (int *)CMSG_DATA(cm);
        for (; Is_block(val_fds); val_fds = Field(val_fds, 1), fds++)
            *fds = Int_val(Field(val_fds, 0));
    };
#else
    if (n_fds > 0) lwt_unix_not_available("fd_passing");
#endif

    int ret = sendmsg(fd, &msg, 0);
    if (ret == -1) uerror("send_msg", Nothing);
    CAMLreturn(Val_int(ret));
}
#endif
