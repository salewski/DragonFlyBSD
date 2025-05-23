/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 *
 * @(#)keyserv.c	1.15	94/04/25 SMI
 * $FreeBSD: src/usr.sbin/keyserv/keyserv.c,v 1.3.2.2 2001/07/19 10:58:22 roam Exp $
 * $DragonFly: src/usr.sbin/keyserv/keyserv.c,v 1.6 2004/12/18 22:48:03 swildner Exp $
 */

/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/*
 * Keyserver
 * Store secret keys per uid. Do public key encryption and decryption
 * operations. Generate "random" keys.
 * Do not talk to anything but a local root
 * process on the local transport only
 */

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/param.h>
#include <sys/file.h>
#include <rpc/des_crypt.h>
#include <rpc/des.h>
#include <rpc/key_prot.h>
#include <rpcsvc/crypt.h>
#include "keyserv.h"

#ifndef NGROUPS
#define	NGROUPS 16
#endif

#ifndef KEYSERVSOCK
#define KEYSERVSOCK "/var/run/keyservsock"
#endif

static void randomize( des_block * );
static void usage( void );
static int getrootkey( des_block *, int );
static int root_auth( SVCXPRT *, struct svc_req * );

#ifdef DEBUG
static int debugging = 1;
#else
static int debugging = 0;
#endif

static void keyprogram();
static des_block masterkey;
char *getenv();
static char ROOTKEY[] = "/etc/.rootkey";

/*
 * Hack to allow the keyserver to use AUTH_DES (for authenticated
 * NIS+ calls, for example).  The only functions that get called
 * are key_encryptsession_pk, key_decryptsession_pk, and key_gendes.
 *
 * The approach is to have the keyserver fill in pointers to local
 * implementations of these functions, and to call those in key_call().
 */

extern cryptkeyres *(*__key_encryptsession_pk_LOCAL)();
extern cryptkeyres *(*__key_decryptsession_pk_LOCAL)();
extern des_block *(*__key_gendes_LOCAL)();
extern int (*__des_crypt_LOCAL)();

cryptkeyres *key_encrypt_pk_2_svc_prog( uid_t, cryptkeyarg2 * );
cryptkeyres *key_decrypt_pk_2_svc_prog( uid_t, cryptkeyarg2 * );
des_block *key_gen_1_svc_prog( void *, struct svc_req * );

int
main(int argc, char **argv)
{
	int nflag = 0;
	int c;
	SVCXPRT *transp;
	int sock = RPC_ANYSOCK;
	int warn = 0;
	char *path = NULL;

	__key_encryptsession_pk_LOCAL = &key_encrypt_pk_2_svc_prog;
	__key_decryptsession_pk_LOCAL = &key_decrypt_pk_2_svc_prog;
	__key_gendes_LOCAL = &key_gen_1_svc_prog;

	while ((c = getopt(argc, argv, "ndDvp:")) != -1)
		switch (c) {
		case 'n':
			nflag++;
			break;
		case 'd':
			pk_nodefaultkeys();
			break;
		case 'D':
			debugging = 1;
			break;
		case 'v':
			warn = 1;
			break;
		case 'p':
			path = optarg;
			break;
		default:
			usage();
		}

	load_des(warn, path);
	__des_crypt_LOCAL = _my_crypt;
	if (svc_auth_reg(AUTH_DES, _svcauth_des) == -1)
		errx(1, "failed to register AUTH_DES authenticator");

	if (optind != argc) {
		usage();
	}

	/*
	 * Initialize
	 */
	umask(066);	/* paranoia */
	if (geteuid() != 0)
		errx(1, "keyserv must be run as root");
	setmodulus(HEXMODULUS);
	getrootkey(&masterkey, nflag);


	/* Create services. */

	pmap_unset(KEY_PROG, KEY_VERS);
	pmap_unset(KEY_PROG, KEY_VERS2);
	unlink(KEYSERVSOCK);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL)
		errx(1, "cannot create udp service");
	if (!svc_register(transp, KEY_PROG, KEY_VERS, keyprogram, IPPROTO_UDP))
		errx(1, "unable to register (KEY_PROG, KEY_VERS, udp)");
	if (!svc_register(transp, KEY_PROG, KEY_VERS2, keyprogram, IPPROTO_UDP))
		errx(1, "unable to register (KEY_PROG, KEY_VERS2, udp)");

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL)
		errx(1, "cannot create tcp service");
	if (!svc_register(transp, KEY_PROG, KEY_VERS, keyprogram, IPPROTO_TCP))
		errx(1, "unable to register (KEY_PROG, KEY_VERS, tcp)");
	if (!svc_register(transp, KEY_PROG, KEY_VERS2, keyprogram, IPPROTO_TCP))
		errx(1, "unable to register (KEY_PROG, KEY_VERS2, tcp)");

	transp = svcunix_create(sock, 0, 0, KEYSERVSOCK);
	chmod(KEYSERVSOCK, 0666);
	if (transp == NULL)
		errx(1, "cannot create AF_UNIX service");
	if (!svc_register(transp, KEY_PROG, KEY_VERS, keyprogram, 0))
		errx(1, "unable to register (KEY_PROG, KEY_VERS, unix)");
	if (!svc_register(transp, KEY_PROG, KEY_VERS2, keyprogram, 0))
		errx(1, "unable to register (KEY_PROG, KEY_VERS2, unix)");
	if (!svc_register(transp, CRYPT_PROG, CRYPT_VERS, crypt_prog_1, 0))
		errx(1, "unable to register (CRYPT_PROG, CRYPT_VERS, unix)");

	if (!debugging) {
		daemon(0,0);
	}

	signal(SIGPIPE, SIG_IGN);

	svc_run();
	abort();
	/* NOTREACHED */
}

/*
 * In the event that we don't get a root password, we try to
 * randomize the master key the best we can
 */
static void
randomize(des_block *master)
{
	int i;
	int seed;
	struct timeval tv;
	int shift;

	seed = 0;
	for (i = 0; i < 1024; i++) {
		gettimeofday(&tv, (struct timezone *) NULL);
		shift = i % 8 * sizeof (int);
		seed ^= (tv.tv_usec << shift) | (tv.tv_usec >> (32 - shift));
	}
#ifdef KEYSERV_RANDOM
	srandom(seed);
	master->key.low = random();
	master->key.high = random();
	srandom(seed);
#else
	/* use stupid dangerous bad rand() */
	srand(seed);
	master->key.low = rand();
	master->key.high = rand();
	srand(seed);
#endif
}

/*
 * Try to get root's secret key, by prompting if terminal is a tty, else trying
 * from standard input.
 * Returns 1 on success.
 */
static int
getrootkey(des_block *master, int prompt)
{
	char *passwd;
	char name[MAXNETNAMELEN + 1];
	char secret[HEXKEYBYTES];
	key_netstarg netstore;
	int fd;

	if (!prompt) {
		/*
		 * Read secret key out of ROOTKEY
		 */
		fd = open(ROOTKEY, O_RDONLY, 0);
		if (fd < 0) {
			randomize(master);
			return (0);
		}
		if (read(fd, secret, HEXKEYBYTES) < HEXKEYBYTES) {
			warnx("the key read from %s was too short", ROOTKEY);
			close(fd);
			return (0);
		}
		close(fd);
		if (!getnetname(name)) {
		    warnx(
	"failed to generate host's netname when establishing root's key");
		    return (0);
		}
		memcpy(netstore.st_priv_key, secret, HEXKEYBYTES);
		memset(netstore.st_pub_key, 0, HEXKEYBYTES);
		netstore.st_netname = name;
		if (pk_netput(0, &netstore) != KEY_SUCCESS) {
		    warnx("could not set root's key and netname");
		    return (0);
		}
		return (1);
	}
	/*
	 * Decrypt yellow pages publickey entry to get secret key
	 */
	passwd = getpass("root password:");
	passwd2des(passwd, (char *)master);
	getnetname(name);
	if (!getsecretkey(name, secret, passwd)) {
		warnx("can't find %s's secret key", name);
		return (0);
	}
	if (secret[0] == 0) {
		warnx("password does not decrypt secret key for %s", name);
		return (0);
	}
	pk_setkey(0, secret);
	/*
	 * Store it for future use in $ROOTKEY, if possible
	 */
	fd = open(ROOTKEY, O_WRONLY|O_TRUNC|O_CREAT, 0);
	if (fd > 0) {
		char newline = '\n';

		write(fd, secret, strlen(secret));
		write(fd, &newline, sizeof (newline));
		close(fd);
	}
	return (1);
}

/*
 * Procedures to implement RPC service
 */
char *
strstatus(keystatus status)
{
	switch (status) {
	case KEY_SUCCESS:
		return ("KEY_SUCCESS");
	case KEY_NOSECRET:
		return ("KEY_NOSECRET");
	case KEY_UNKNOWN:
		return ("KEY_UNKNOWN");
	case KEY_SYSTEMERR:
		return ("KEY_SYSTEMERR");
	default:
		return ("(bad result code)");
	}
}

keystatus *
key_set_1_svc_prog(uid_t uid, keybuf key)
{
	static keystatus status;

	if (debugging) {
		fprintf(stderr, "set(%ld, %.*s) = ", uid,
				(int) sizeof (keybuf), key);
	}
	status = pk_setkey(uid, key);
	if (debugging) {
		fprintf(stderr, "%s\n", strstatus(status));
		fflush(stderr);
	}
	return (&status);
}

cryptkeyres *
key_encrypt_pk_2_svc_prog(uid_t uid, cryptkeyarg2 *arg)
{
	static cryptkeyres res;

	if (debugging) {
		fprintf(stderr, "encrypt(%ld, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res.cryptkeyres_u.deskey = arg->deskey;
	res.status = pk_encrypt(uid, arg->remotename, &(arg->remotekey),
				&res.cryptkeyres_u.deskey);
	if (debugging) {
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "%08x%08x\n",
					res.cryptkeyres_u.deskey.key.high,
					res.cryptkeyres_u.deskey.key.low);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}

cryptkeyres *
key_decrypt_pk_2_svc_prog(uid_t uid, cryptkeyarg2 *arg)
{
	static cryptkeyres res;

	if (debugging) {
		fprintf(stderr, "decrypt(%ld, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res.cryptkeyres_u.deskey = arg->deskey;
	res.status = pk_decrypt(uid, arg->remotename, &(arg->remotekey),
				&res.cryptkeyres_u.deskey);
	if (debugging) {
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "%08x%08x\n",
					res.cryptkeyres_u.deskey.key.high,
					res.cryptkeyres_u.deskey.key.low);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}

keystatus *
key_net_put_2_svc_prog(uid_t uid, key_netstarg *arg)
{
	static keystatus status;

	if (debugging) {
		fprintf(stderr, "net_put(%s, %.*s, %.*s) = ",
			arg->st_netname, (int)sizeof (arg->st_pub_key),
			arg->st_pub_key, (int)sizeof (arg->st_priv_key),
			arg->st_priv_key);
	};

	status = pk_netput(uid, arg);

	if (debugging) {
		fprintf(stderr, "%s\n", strstatus(status));
		fflush(stderr);
	}

	return (&status);
}

key_netstres *
key_net_get_2_svc_prog(uid_t uid, void *arg)
{
	static key_netstres keynetname;

	if (debugging)
		fprintf(stderr, "net_get(%ld) = ", uid);

	keynetname.status = pk_netget(uid, &keynetname.key_netstres_u.knet);
	if (debugging) {
		if (keynetname.status == KEY_SUCCESS) {
			fprintf(stderr, "<%s, %.*s, %.*s>\n",
			keynetname.key_netstres_u.knet.st_netname,
			(int)sizeof (keynetname.key_netstres_u.knet.st_pub_key),
			keynetname.key_netstres_u.knet.st_pub_key,
			(int)sizeof (keynetname.key_netstres_u.knet.st_priv_key),
			keynetname.key_netstres_u.knet.st_priv_key);
		} else {
			fprintf(stderr, "NOT FOUND\n");
		}
		fflush(stderr);
	}

	return (&keynetname);

}

cryptkeyres *
key_get_conv_2_svc_prog(uid_t uid, keybuf arg)
{
	static cryptkeyres  res;

	if (debugging)
		fprintf(stderr, "get_conv(%ld, %.*s) = ", uid,
			(int)sizeof (arg), arg);


	res.status = pk_get_conv_key(uid, arg, &res);

	if (debugging) {
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "%08x%08x\n",
				res.cryptkeyres_u.deskey.key.high,
				res.cryptkeyres_u.deskey.key.low);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}


cryptkeyres *
key_encrypt_1_svc_prog(uid_t uid, cryptkeyarg *arg)
{
	static cryptkeyres res;

	if (debugging) {
		fprintf(stderr, "encrypt(%ld, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res.cryptkeyres_u.deskey = arg->deskey;
	res.status = pk_encrypt(uid, arg->remotename, NULL,
				&res.cryptkeyres_u.deskey);
	if (debugging) {
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "%08x%08x\n",
					res.cryptkeyres_u.deskey.key.high,
					res.cryptkeyres_u.deskey.key.low);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}

cryptkeyres *
key_decrypt_1_svc_prog(uid_t uid, cryptkeyarg *arg)
{
	static cryptkeyres res;

	if (debugging) {
		fprintf(stderr, "decrypt(%ld, %s, %08x%08x) = ", uid,
				arg->remotename, arg->deskey.key.high,
				arg->deskey.key.low);
	}
	res.cryptkeyres_u.deskey = arg->deskey;
	res.status = pk_decrypt(uid, arg->remotename, NULL,
				&res.cryptkeyres_u.deskey);
	if (debugging) {
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "%08x%08x\n",
					res.cryptkeyres_u.deskey.key.high,
					res.cryptkeyres_u.deskey.key.low);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}

/* ARGSUSED */
des_block *
key_gen_1_svc_prog(void *v, struct svc_req *s)
{
	struct timeval time;
	static des_block keygen;
	static des_block key;

	gettimeofday(&time, (struct timezone *) NULL);
	keygen.key.high += (time.tv_sec ^ time.tv_usec);
	keygen.key.low += (time.tv_sec ^ time.tv_usec);
	ecb_crypt((char *)&masterkey, (char *)&keygen, sizeof (keygen),
		DES_ENCRYPT | DES_HW);
	key = keygen;
	des_setparity((char *)&key);
	if (debugging) {
		fprintf(stderr, "gen() = %08x%08x\n", key.key.high,
					key.key.low);
		fflush(stderr);
	}
	return (&key);
}

getcredres *
key_getcred_1_svc_prog(uid_t uid, netnamestr *name)
{
	static getcredres res;
	static u_int gids[NGROUPS];
	struct unixcred *cred;

	cred = &res.getcredres_u.cred;
	cred->gids.gids_val = gids;
	if (!netname2user(*name, (uid_t *) &cred->uid, (gid_t *) &cred->gid,
			(int *)&cred->gids.gids_len, (gid_t *)gids)) {
		res.status = KEY_UNKNOWN;
	} else {
		res.status = KEY_SUCCESS;
	}
	if (debugging) {
		fprintf(stderr, "getcred(%s) = ", *name);
		if (res.status == KEY_SUCCESS) {
			fprintf(stderr, "uid=%d, gid=%d, grouplen=%d\n",
				cred->uid, cred->gid, cred->gids.gids_len);
		} else {
			fprintf(stderr, "%s\n", strstatus(res.status));
		}
		fflush(stderr);
	}
	return (&res);
}

/*
 * RPC boilerplate
 */
static void
keyprogram(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		keybuf key_set_1_arg;
		cryptkeyarg key_encrypt_1_arg;
		cryptkeyarg key_decrypt_1_arg;
		netnamestr key_getcred_1_arg;
		cryptkeyarg key_encrypt_2_arg;
		cryptkeyarg key_decrypt_2_arg;
		netnamestr key_getcred_2_arg;
		cryptkeyarg2 key_encrypt_pk_2_arg;
		cryptkeyarg2 key_decrypt_pk_2_arg;
		key_netstarg key_net_put_2_arg;
		netobj  key_get_conv_2_arg;
	} argument;
	char *result;
	bool_t(*xdr_argument)(), (*xdr_result)();
	char *(*local) ();
	uid_t uid = -1;
	int check_auth;

	switch (rqstp->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case KEY_SET:
		xdr_argument = xdr_keybuf;
		xdr_result = xdr_int;
		local = (char *(*)()) key_set_1_svc_prog;
		check_auth = 1;
		break;

	case KEY_ENCRYPT:
		xdr_argument = xdr_cryptkeyarg;
		xdr_result = xdr_cryptkeyres;
		local = (char *(*)()) key_encrypt_1_svc_prog;
		check_auth = 1;
		break;

	case KEY_DECRYPT:
		xdr_argument = xdr_cryptkeyarg;
		xdr_result = xdr_cryptkeyres;
		local = (char *(*)()) key_decrypt_1_svc_prog;
		check_auth = 1;
		break;

	case KEY_GEN:
		xdr_argument = xdr_void;
		xdr_result = xdr_des_block;
		local = (char *(*)()) key_gen_1_svc_prog;
		check_auth = 0;
		break;

	case KEY_GETCRED:
		xdr_argument = xdr_netnamestr;
		xdr_result = xdr_getcredres;
		local = (char *(*)()) key_getcred_1_svc_prog;
		check_auth = 0;
		break;

	case KEY_ENCRYPT_PK:
		xdr_argument = xdr_cryptkeyarg2;
		xdr_result = xdr_cryptkeyres;
		local = (char *(*)()) key_encrypt_pk_2_svc_prog;
		check_auth = 1;
		break;

	case KEY_DECRYPT_PK:
		xdr_argument = xdr_cryptkeyarg2;
		xdr_result = xdr_cryptkeyres;
		local = (char *(*)()) key_decrypt_pk_2_svc_prog;
		check_auth = 1;
		break;


	case KEY_NET_PUT:
		xdr_argument = xdr_key_netstarg;
		xdr_result = xdr_keystatus;
		local = (char *(*)()) key_net_put_2_svc_prog;
		check_auth = 1;
		break;

	case KEY_NET_GET:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = xdr_key_netstres;
		local = (char *(*)()) key_net_get_2_svc_prog;
		check_auth = 1;
		break;

	case KEY_GET_CONV:
		xdr_argument = (xdrproc_t) xdr_keybuf;
		xdr_result = xdr_cryptkeyres;
		local = (char *(*)()) key_get_conv_2_svc_prog;
		check_auth = 1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	if (check_auth) {
		if (root_auth(transp, rqstp) == 0) {
			if (debugging) {
				fprintf(stderr,
					"not local privileged process\n");
			}
			svcerr_weakauth(transp);
			return;
		}
		if (rqstp->rq_cred.oa_flavor != AUTH_SYS) {
			if (debugging) {
				fprintf(stderr, "not unix authentication\n");
			}
			svcerr_weakauth(transp);
			return;
		}
		uid = ((struct authsys_parms *)rqstp->rq_clntcred)->aup_uid;
	}

	memset((char *) &argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local) (uid, &argument);
	if (!svc_sendreply(transp, xdr_result, (char *) result)) {
		if (debugging)
			fprintf(stderr, "unable to reply\n");
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		if (debugging)
			fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	return;
}

static int
root_auth(SVCXPRT *trans, struct svc_req *rqstp)
{
	uid_t uid;
	struct sockaddr_in *remote;

	remote = svc_getcaller(trans);
	if (remote->sin_family == AF_INET) {
		if (debugging)
			fprintf(stderr, "client didn't use AF_UNIX\n");
		return (0);
	}

	if (__rpc_get_local_uid(&uid, trans) < 0) {
		if (debugging)
			fprintf(stderr, "__rpc_get_local_uid failed\n");
		return (0);
	}

	if (debugging)
		fprintf(stderr, "local_uid  %ld\n", uid);
	if (uid == 0)
		return (1);
	if (rqstp->rq_cred.oa_flavor == AUTH_SYS) {
		if (((uid_t) ((struct authunix_parms *)
			rqstp->rq_clntcred)->aup_uid)
			== uid) {
			return (1);
		} else {
			if (debugging)
				fprintf(stderr,
			"local_uid  %ld mismatches auth %ld\n", uid,
((uid_t) ((struct authunix_parms *)rqstp->rq_clntcred)->aup_uid));
			return (0);
		}
	} else {
		if (debugging)
			fprintf(stderr, "Not auth sys\n");
		return (0);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: keyserv [-n] [-D] [-d] [-v] [-p path]\n");
	fprintf(stderr, "-d disables the use of default keys\n");
	exit(1);
}
