/*
 * Copyright (C) 2004, 2005, 2007, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
 * Copyright 2015 by the NTPsec project contributors
 * SPDX-License-Identifier: ISC
 */

#include <config.h>

#include <winsock2.h>
#include "errno2result.h"
#include <isc/result.h>
#include <isc/util.h>

/*
 * Convert a POSIX errno value into an isc_result_t.  The
 * list of supported errno values is not complete; new users
 * of this function should add any expected errors that are
 * not already there.
 */
isc_result_t
isc__errno2resultx(int posixerrno, const char *file, int line) {
	char strbuf[BUFSIZ];

	switch (posixerrno) {
	case ENOTDIR:
	case WSAELOOP:
	case WSAEINVAL:
	case EINVAL:		/* XXX sometimes this is not for files */
	case ENAMETOOLONG:
	case WSAENAMETOOLONG:
	case EBADF:
	case WSAEBADF:
		return (ISC_R_INVALIDFILE);
	case ENOENT:
		return (ISC_R_FILENOTFOUND);
	case EACCES:
	case WSAEACCES:
	case EPERM:
		return (ISC_R_NOPERM);
	case EEXIST:
		return (ISC_R_FILEEXISTS);
	case EIO:
		return (ISC_R_IOERROR);
	case ENOMEM:
		return (ISC_R_NOMEMORY);
	case ENFILE:
	case EMFILE:
	case WSAEMFILE:
		return (ISC_R_TOOMANYOPENFILES);
	case ERROR_CANCELLED:
		return (ISC_R_CANCELED);
	case ERROR_CONNECTION_REFUSED:
	case WSAECONNREFUSED:
		return (ISC_R_CONNREFUSED);
	case WSAENOTCONN:
	case ERROR_CONNECTION_INVALID:
		return (ISC_R_NOTCONNECTED);
	case ERROR_HOST_UNREACHABLE:
	case WSAEHOSTUNREACH:
		return (ISC_R_HOSTUNREACH);
	case ERROR_NETWORK_UNREACHABLE:
	case WSAENETUNREACH:
		return (ISC_R_NETUNREACH);
	case ERROR_NO_NETWORK:
		return (ISC_R_NETUNREACH);
	case ERROR_PORT_UNREACHABLE:
		return (ISC_R_HOSTUNREACH);
	case ERROR_SEM_TIMEOUT:
		return (ISC_R_TIMEDOUT);
	case WSAECONNRESET:
	case WSAENETRESET:
	case WSAECONNABORTED:
	case WSAEDISCON:
	case ERROR_OPERATION_ABORTED:
	case ERROR_CONNECTION_ABORTED:
	case ERROR_REQUEST_ABORTED:
		return (ISC_R_CONNECTIONRESET);
	case WSAEADDRNOTAVAIL:
		return (ISC_R_ADDRNOTAVAIL);
	case ERROR_NETNAME_DELETED:
	case WSAENETDOWN:
		return (ISC_R_NETUNREACH);
	case WSAEHOSTDOWN:
		return (ISC_R_HOSTUNREACH);
	case WSAENOBUFS:
		return (ISC_R_NORESOURCES);
	default:
		strerror_r(posixerrno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(file, line, "unable to convert errno "
				 "to isc_result: %d: %s", posixerrno, strbuf);
		/*
		 * XXXDCL would be nice if perhaps this function could
		 * return the system's error string, so the caller
		 * might have something more descriptive than "unexpected
		 * error" to log with.
		 */
		return (ISC_R_UNEXPECTED);
	}
}
