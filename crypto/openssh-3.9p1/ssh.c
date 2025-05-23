/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Ssh client program.  This program can be used to log into a remote machine.
 * The software supports strong authentication, encryption, and forwarding
 * of X11, TCP/IP, and authentication connections.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 2000, 2001, 2002, 2003 Markus Friedl.  All rights reserved.
 *
 * Modified to work with SSL by Niels Provos <provos@citi.umich.edu>
 * in Canada (German citizen).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: ssh.c,v 1.224 2004/07/28 09:40:29 markus Exp $");

#include <openssl/evp.h>
#include <openssl/err.h>

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "compat.h"
#include "cipher.h"
#include "xmalloc.h"
#include "packet.h"
#include "buffer.h"
#include "bufaux.h"
#include "channels.h"
#include "key.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "dispatch.h"
#include "clientloop.h"
#include "log.h"
#include "readconf.h"
#include "sshconnect.h"
#include "misc.h"
#include "kex.h"
#include "mac.h"
#include "sshpty.h"
#include "match.h"
#include "msg.h"
#include "monitor_fdpass.h"
#include "uidswap.h"

#ifdef SMARTCARD
#include "scard.h"
#endif

extern char *__progname;

/* Flag indicating whether debug mode is on.  This can be set on the command line. */
int debug_flag = 0;

/* Flag indicating whether a tty should be allocated */
int tty_flag = 0;
int no_tty_flag = 0;
int force_tty_flag = 0;

/* don't exec a shell */
int no_shell_flag = 0;

/*
 * Flag indicating that nothing should be read from stdin.  This can be set
 * on the command line.
 */
int stdin_null_flag = 0;

/*
 * Flag indicating that ssh should fork after authentication.  This is useful
 * so that the passphrase can be entered manually, and then ssh goes to the
 * background.
 */
int fork_after_authentication_flag = 0;

/*
 * General data structure for command line options and options configurable
 * in configuration files.  See readconf.h.
 */
Options options;

/* optional user configfile */
char *config = NULL;

/*
 * Name of the host we are connecting to.  This is the name given on the
 * command line, or the HostName specified for the user-supplied name in a
 * configuration file.
 */
char *host;

/* socket address the host resolves to */
struct sockaddr_storage hostaddr;

/* Private host keys. */
Sensitive sensitive_data;

/* Original real UID. */
uid_t original_real_uid;
uid_t original_effective_uid;

/* command to be executed */
Buffer command;

/* Should we execute a command or invoke a subsystem? */
int subsystem_flag = 0;

/* # of replies received for global requests */
static int client_global_request_id = 0;

/* pid of proxycommand child process */
pid_t proxy_command_pid = 0;

/* fd to control socket */
int control_fd = -1;

/* Only used in control client mode */
volatile sig_atomic_t control_client_terminate = 0;
u_int control_server_pid = 0;

/* Prints a help message to the user.  This function never returns. */

static void
usage(void)
{
	fprintf(stderr,
"usage: ssh [-1246AaCfghkMNnqsTtVvXxY] [-b bind_address] [-c cipher_spec]\n"
"           [-D port] [-e escape_char] [-F configfile] [-i identity_file]\n"
"           [-L port:host:hostport] [-l login_name] [-m mac_spec] [-o option]\n"
"           [-p port] [-R port:host:hostport] [-S ctl] [user@]hostname [command]\n"
	);
	exit(1);
}

static int ssh_session(void);
static int ssh_session2(void);
static void load_public_identity_files(void);
static void control_client(const char *path);

/*
 * Main program for the ssh client.
 */
int
main(int ac, char **av)
{
	int i, opt, exit_status;
	u_short fwd_port, fwd_host_port;
	char sfwd_port[6], sfwd_host_port[6];
	char *p, *cp, *line, buf[256];
	struct stat st;
	struct passwd *pw;
	int dummy;
	extern int optind, optreset;
	extern char *optarg;

	__progname = ssh_get_progname(av[0]);
	init_rng();

	/*
	 * Save the original real uid.  It will be needed later (uid-swapping
	 * may clobber the real uid).
	 */
	original_real_uid = getuid();
	original_effective_uid = geteuid();

	/*
	 * Use uid-swapping to give up root privileges for the duration of
	 * option processing.  We will re-instantiate the rights when we are
	 * ready to create the privileged port, and will permanently drop
	 * them when the port has been created (actually, when the connection
	 * has been made, as we may need to create the port several times).
	 */
	PRIV_END;

#ifdef HAVE_SETRLIMIT
	/* If we are installed setuid root be careful to not drop core. */
	if (original_real_uid != original_effective_uid) {
		struct rlimit rlim;
		rlim.rlim_cur = rlim.rlim_max = 0;
		if (setrlimit(RLIMIT_CORE, &rlim) < 0)
			fatal("setrlimit failed: %.100s", strerror(errno));
	}
#endif
	/* Get user data. */
	pw = getpwuid(original_real_uid);
	if (!pw) {
		logit("You don't exist, go away!");
		exit(1);
	}
	/* Take a copy of the returned structure. */
	pw = pwcopy(pw);

	/*
	 * Set our umask to something reasonable, as some files are created
	 * with the default umask.  This will make them world-readable but
	 * writable only by the owner, which is ok for all files for which we
	 * don't set the modes explicitly.
	 */
	umask(022);

	/* Initialize option structure to indicate that no values have been set. */
	initialize_options(&options);

	/* Parse command-line arguments. */
	host = NULL;

again:
	while ((opt = getopt(ac, av,
	    "1246ab:c:e:fgi:kl:m:no:p:qstvxACD:F:I:L:MNPR:S:TVXY")) != -1) {
		switch (opt) {
		case '1':
			options.protocol = SSH_PROTO_1;
			break;
		case '2':
			options.protocol = SSH_PROTO_2;
			break;
		case '4':
			options.address_family = AF_INET;
			break;
		case '6':
			options.address_family = AF_INET6;
			break;
		case 'n':
			stdin_null_flag = 1;
			break;
		case 'f':
			fork_after_authentication_flag = 1;
			stdin_null_flag = 1;
			break;
		case 'x':
			options.forward_x11 = 0;
			break;
		case 'X':
			options.forward_x11 = 1;
			break;
		case 'Y':
			options.forward_x11 = 1;
			options.forward_x11_trusted = 1;
			break;
		case 'g':
			options.gateway_ports = 1;
			break;
		case 'P':	/* deprecated */
			options.use_privileged_port = 0;
			break;
		case 'a':
			options.forward_agent = 0;
			break;
		case 'A':
			options.forward_agent = 1;
			break;
		case 'k':
			options.gss_deleg_creds = 0;
			break;
		case 'i':
			if (stat(optarg, &st) < 0) {
				fprintf(stderr, "Warning: Identity file %s "
				    "does not exist.\n", optarg);
				break;
			}
			if (options.num_identity_files >=
			    SSH_MAX_IDENTITY_FILES)
				fatal("Too many identity files specified "
				    "(max %d)", SSH_MAX_IDENTITY_FILES);
			options.identity_files[options.num_identity_files++] =
			    xstrdup(optarg);
			break;
		case 'I':
#ifdef SMARTCARD
			options.smartcard_device = xstrdup(optarg);
#else
			fprintf(stderr, "no support for smartcards.\n");
#endif
			break;
		case 't':
			if (tty_flag)
				force_tty_flag = 1;
			tty_flag = 1;
			break;
		case 'v':
			if (debug_flag == 0) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else {
				if (options.log_level < SYSLOG_LEVEL_DEBUG3)
					options.log_level++;
				break;
			}
			/* fallthrough */
		case 'V':
			fprintf(stderr, "%s, %s\n",
			    SSH_VERSION, SSLeay_version(SSLEAY_VERSION));
			if (opt == 'V')
				exit(0);
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'e':
			if (optarg[0] == '^' && optarg[2] == 0 &&
			    (u_char) optarg[1] >= 64 &&
			    (u_char) optarg[1] < 128)
				options.escape_char = (u_char) optarg[1] & 31;
			else if (strlen(optarg) == 1)
				options.escape_char = (u_char) optarg[0];
			else if (strcmp(optarg, "none") == 0)
				options.escape_char = SSH_ESCAPECHAR_NONE;
			else {
				fprintf(stderr, "Bad escape character '%s'.\n",
				    optarg);
				exit(1);
			}
			break;
		case 'c':
			if (ciphers_valid(optarg)) {
				/* SSH2 only */
				options.ciphers = xstrdup(optarg);
				options.cipher = SSH_CIPHER_INVALID;
			} else {
				/* SSH1 only */
				options.cipher = cipher_number(optarg);
				if (options.cipher == -1) {
					fprintf(stderr,
					    "Unknown cipher type '%s'\n",
					    optarg);
					exit(1);
				}
				if (options.cipher == SSH_CIPHER_3DES)
					options.ciphers = "3des-cbc";
				else if (options.cipher == SSH_CIPHER_BLOWFISH)
					options.ciphers = "blowfish-cbc";
				else
					options.ciphers = (char *)-1;
			}
			break;
		case 'm':
			if (mac_valid(optarg))
				options.macs = xstrdup(optarg);
			else {
				fprintf(stderr, "Unknown mac type '%s'\n",
				    optarg);
				exit(1);
			}
			break;
		case 'M':
			options.control_master =
			    (options.control_master >= 1) ? 2 : 1;
			break;
		case 'p':
			options.port = a2port(optarg);
			if (options.port == 0) {
				fprintf(stderr, "Bad port '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'l':
			options.user = optarg;
			break;

		case 'L':
		case 'R':
			if (sscanf(optarg, "%5[0123456789]:%255[^:]:%5[0123456789]",
			    sfwd_port, buf, sfwd_host_port) != 3 &&
			    sscanf(optarg, "%5[0123456789]/%255[^/]/%5[0123456789]",
			    sfwd_port, buf, sfwd_host_port) != 3) {
				fprintf(stderr,
				    "Bad forwarding specification '%s'\n",
				    optarg);
				usage();
				/* NOTREACHED */
			}
			if ((fwd_port = a2port(sfwd_port)) == 0 ||
			    (fwd_host_port = a2port(sfwd_host_port)) == 0) {
				fprintf(stderr,
				    "Bad forwarding port(s) '%s'\n", optarg);
				exit(1);
			}
			if (opt == 'L')
				add_local_forward(&options, fwd_port, buf,
				    fwd_host_port);
			else if (opt == 'R')
				add_remote_forward(&options, fwd_port, buf,
				    fwd_host_port);
			break;

		case 'D':
			fwd_port = a2port(optarg);
			if (fwd_port == 0) {
				fprintf(stderr, "Bad dynamic port '%s'\n",
				    optarg);
				exit(1);
			}
			add_local_forward(&options, fwd_port, "socks", 0);
			break;

		case 'C':
			options.compression = 1;
			break;
		case 'N':
			no_shell_flag = 1;
			no_tty_flag = 1;
			break;
		case 'T':
			no_tty_flag = 1;
			break;
		case 'o':
			dummy = 1;
			line = xstrdup(optarg);
			if (process_config_line(&options, host ? host : "",
			    line, "command-line", 0, &dummy) != 0)
				exit(1);
			xfree(line);
			break;
		case 's':
			subsystem_flag = 1;
			break;
		case 'S':
			if (options.control_path != NULL)
				free(options.control_path);
			options.control_path = xstrdup(optarg);
			break;
		case 'b':
			options.bind_address = optarg;
			break;
		case 'F':
			config = optarg;
			break;
		default:
			usage();
		}
	}

	ac -= optind;
	av += optind;

	if (ac > 0 && !host && **av != '-') {
		if (strrchr(*av, '@')) {
			p = xstrdup(*av);
			cp = strrchr(p, '@');
			if (cp == NULL || cp == p)
				usage();
			options.user = p;
			*cp = '\0';
			host = ++cp;
		} else
			host = *av;
		if (ac > 1) {
			optind = optreset = 1;
			goto again;
		}
		ac--, av++;
	}

	/* Check that we got a host name. */
	if (!host)
		usage();

	SSLeay_add_all_algorithms();
	ERR_load_crypto_strings();

	/* Initialize the command to execute on remote host. */
	buffer_init(&command);

	/*
	 * Save the command to execute on the remote host in a buffer. There
	 * is no limit on the length of the command, except by the maximum
	 * packet size.  Also sets the tty flag if there is no command.
	 */
	if (!ac) {
		/* No command specified - execute shell on a tty. */
		tty_flag = 1;
		if (subsystem_flag) {
			fprintf(stderr,
			    "You must specify a subsystem to invoke.\n");
			usage();
		}
	} else {
		/* A command has been specified.  Store it into the buffer. */
		for (i = 0; i < ac; i++) {
			if (i)
				buffer_append(&command, " ", 1);
			buffer_append(&command, av[i], strlen(av[i]));
		}
	}

	/* Cannot fork to background if no command. */
	if (fork_after_authentication_flag && buffer_len(&command) == 0 && !no_shell_flag)
		fatal("Cannot fork into background without a command to execute.");

	/* Allocate a tty by default if no command specified. */
	if (buffer_len(&command) == 0)
		tty_flag = 1;

	/* Force no tty */
	if (no_tty_flag)
		tty_flag = 0;
	/* Do not allocate a tty if stdin is not a tty. */
	if (!isatty(fileno(stdin)) && !force_tty_flag) {
		if (tty_flag)
			logit("Pseudo-terminal will not be allocated because stdin is not a terminal.");
		tty_flag = 0;
	}

	/*
	 * Initialize "log" output.  Since we are the client all output
	 * actually goes to stderr.
	 */
	log_init(av[0], options.log_level == -1 ? SYSLOG_LEVEL_INFO : options.log_level,
	    SYSLOG_FACILITY_USER, 1);

	/*
	 * Read per-user configuration file.  Ignore the system wide config
	 * file if the user specifies a config file on the command line.
	 */
	if (config != NULL) {
		if (!read_config_file(config, host, &options, 0))
			fatal("Can't open user config file %.100s: "
			    "%.100s", config, strerror(errno));
	} else  {
		snprintf(buf, sizeof buf, "%.100s/%.100s", pw->pw_dir,
		    _PATH_SSH_USER_CONFFILE);
		(void)read_config_file(buf, host, &options, 1);

		/* Read systemwide configuration file after use config. */
		(void)read_config_file(_PATH_HOST_CONFIG_FILE, host,
		    &options, 0);
	}

	/* Fill configuration defaults. */
	fill_default_options(&options);

	channel_set_af(options.address_family);

	/* reinit */
	log_init(av[0], options.log_level, SYSLOG_FACILITY_USER, 1);

	seed_rng();

	if (options.user == NULL)
		options.user = xstrdup(pw->pw_name);

	if (options.hostname != NULL)
		host = options.hostname;

	/* force lowercase for hostkey matching */
	if (options.host_key_alias != NULL) {
		for (p = options.host_key_alias; *p; p++)
			if (isupper(*p))
				*p = tolower(*p);
	}

	if (options.proxy_command != NULL &&
	    strcmp(options.proxy_command, "none") == 0)
		options.proxy_command = NULL;

	if (options.control_path != NULL) {
		options.control_path = tilde_expand_filename(
		   options.control_path, original_real_uid);
	}
	if (options.control_path != NULL && options.control_master == 0)
		control_client(options.control_path); /* This doesn't return */

	/* Open a connection to the remote host. */
	if (ssh_connect(host, &hostaddr, options.port,
	    options.address_family, options.connection_attempts,
#ifdef HAVE_CYGWIN
	    options.use_privileged_port,
#else
	    original_effective_uid == 0 && options.use_privileged_port,
#endif
	    options.proxy_command) != 0)
		exit(1);

	/*
	 * If we successfully made the connection, load the host private key
	 * in case we will need it later for combined rsa-rhosts
	 * authentication. This must be done before releasing extra
	 * privileges, because the file is only readable by root.
	 * If we cannot access the private keys, load the public keys
	 * instead and try to execute the ssh-keysign helper instead.
	 */
	sensitive_data.nkeys = 0;
	sensitive_data.keys = NULL;
	sensitive_data.external_keysign = 0;
	if (options.rhosts_rsa_authentication ||
	    options.hostbased_authentication) {
		sensitive_data.nkeys = 3;
		sensitive_data.keys = xmalloc(sensitive_data.nkeys *
		    sizeof(Key));

		PRIV_START;
		sensitive_data.keys[0] = key_load_private_type(KEY_RSA1,
		    _PATH_HOST_KEY_FILE, "", NULL);
		sensitive_data.keys[1] = key_load_private_type(KEY_DSA,
		    _PATH_HOST_DSA_KEY_FILE, "", NULL);
		sensitive_data.keys[2] = key_load_private_type(KEY_RSA,
		    _PATH_HOST_RSA_KEY_FILE, "", NULL);
		PRIV_END;

		if (options.hostbased_authentication == 1 &&
		    sensitive_data.keys[0] == NULL &&
		    sensitive_data.keys[1] == NULL &&
		    sensitive_data.keys[2] == NULL) {
			sensitive_data.keys[1] = key_load_public(
			    _PATH_HOST_DSA_KEY_FILE, NULL);
			sensitive_data.keys[2] = key_load_public(
			    _PATH_HOST_RSA_KEY_FILE, NULL);
			sensitive_data.external_keysign = 1;
		}
	}
	/*
	 * Get rid of any extra privileges that we may have.  We will no
	 * longer need them.  Also, extra privileges could make it very hard
	 * to read identity files and other non-world-readable files from the
	 * user's home directory if it happens to be on a NFS volume where
	 * root is mapped to nobody.
	 */
	if (original_effective_uid == 0) {
		PRIV_START;
		permanently_set_uid(pw);
	}

	/*
	 * Now that we are back to our own permissions, create ~/.ssh
	 * directory if it doesn\'t already exist.
	 */
	snprintf(buf, sizeof buf, "%.100s%s%.100s", pw->pw_dir, strcmp(pw->pw_dir, "/") ? "/" : "", _PATH_SSH_USER_DIR);
	if (stat(buf, &st) < 0)
		if (mkdir(buf, 0700) < 0)
			error("Could not create directory '%.200s'.", buf);

	/* load options.identity_files */
	load_public_identity_files();

	/* Expand ~ in known host file names. */
	/* XXX mem-leaks: */
	options.system_hostfile =
	    tilde_expand_filename(options.system_hostfile, original_real_uid);
	options.user_hostfile =
	    tilde_expand_filename(options.user_hostfile, original_real_uid);
	options.system_hostfile2 =
	    tilde_expand_filename(options.system_hostfile2, original_real_uid);
	options.user_hostfile2 =
	    tilde_expand_filename(options.user_hostfile2, original_real_uid);

	signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE early */

	/* Log into the remote system.  This never returns if the login fails. */
	ssh_login(&sensitive_data, host, (struct sockaddr *)&hostaddr, pw);

	/* We no longer need the private host keys.  Clear them now. */
	if (sensitive_data.nkeys != 0) {
		for (i = 0; i < sensitive_data.nkeys; i++) {
			if (sensitive_data.keys[i] != NULL) {
				/* Destroys contents safely */
				debug3("clear hostkey %d", i);
				key_free(sensitive_data.keys[i]);
				sensitive_data.keys[i] = NULL;
			}
		}
		xfree(sensitive_data.keys);
	}
	for (i = 0; i < options.num_identity_files; i++) {
		if (options.identity_files[i]) {
			xfree(options.identity_files[i]);
			options.identity_files[i] = NULL;
		}
		if (options.identity_keys[i]) {
			key_free(options.identity_keys[i]);
			options.identity_keys[i] = NULL;
		}
	}

	exit_status = compat20 ? ssh_session2() : ssh_session();
	packet_close();

	if (options.control_path != NULL && control_fd != -1)
		unlink(options.control_path);

	/*
	 * Send SIGHUP to proxy command if used. We don't wait() in
	 * case it hangs and instead rely on init to reap the child
	 */
	if (proxy_command_pid > 1)
		kill(proxy_command_pid, SIGHUP);

	return exit_status;
}

#define SSH_X11_PROTO "MIT-MAGIC-COOKIE-1"

static void
x11_get_proto(char **_proto, char **_data)
{
	char cmd[1024];
	char line[512];
	char xdisplay[512];
	static char proto[512], data[512];
	FILE *f;
	int got_data = 0, generated = 0, do_unlink = 0, i;
	char *display, *xauthdir, *xauthfile;
	struct stat st;

	xauthdir = xauthfile = NULL;
	*_proto = proto;
	*_data = data;
	proto[0] = data[0] = '\0';

	if (!options.xauth_location ||
	    (stat(options.xauth_location, &st) == -1)) {
		debug("No xauth program.");
	} else {
		if ((display = getenv("DISPLAY")) == NULL) {
			debug("x11_get_proto: DISPLAY not set");
			return;
		}
		/*
		 * Handle FamilyLocal case where $DISPLAY does
		 * not match an authorization entry.  For this we
		 * just try "xauth list unix:displaynum.screennum".
		 * XXX: "localhost" match to determine FamilyLocal
		 *      is not perfect.
		 */
		if (strncmp(display, "localhost:", 10) == 0) {
			snprintf(xdisplay, sizeof(xdisplay), "unix:%s",
			    display + 10);
			display = xdisplay;
		}
		if (options.forward_x11_trusted == 0) {
			xauthdir = xmalloc(MAXPATHLEN);
			xauthfile = xmalloc(MAXPATHLEN);
			strlcpy(xauthdir, "/tmp/ssh-XXXXXXXXXX", MAXPATHLEN);
			if (mkdtemp(xauthdir) != NULL) {
				do_unlink = 1;
				snprintf(xauthfile, MAXPATHLEN, "%s/xauthfile",
				    xauthdir);
				snprintf(cmd, sizeof(cmd),
				    "%s -f %s generate %s " SSH_X11_PROTO
				    " untrusted timeout 1200 2>" _PATH_DEVNULL,
				    options.xauth_location, xauthfile, display);
				debug2("x11_get_proto: %s", cmd);
				if (system(cmd) == 0)
					generated = 1;
			}
		}
		snprintf(cmd, sizeof(cmd),
		    "%s %s%s list %s . 2>" _PATH_DEVNULL,
		    options.xauth_location,
		    generated ? "-f " : "" ,
		    generated ? xauthfile : "",
		    display);
		debug2("x11_get_proto: %s", cmd);
		f = popen(cmd, "r");
		if (f && fgets(line, sizeof(line), f) &&
		    sscanf(line, "%*s %511s %511s", proto, data) == 2)
			got_data = 1;
		if (f)
			pclose(f);
	}

	if (do_unlink) {
		unlink(xauthfile);
		rmdir(xauthdir);
	}
	if (xauthdir)
		xfree(xauthdir);
	if (xauthfile)
		xfree(xauthfile);

	/*
	 * If we didn't get authentication data, just make up some
	 * data.  The forwarding code will check the validity of the
	 * response anyway, and substitute this data.  The X11
	 * server, however, will ignore this fake data and use
	 * whatever authentication mechanisms it was using otherwise
	 * for the local connection.
	 */
	if (!got_data) {
		u_int32_t rnd = 0;

		logit("Warning: No xauth data; "
		    "using fake authentication data for X11 forwarding.");
		strlcpy(proto, SSH_X11_PROTO, sizeof proto);
		for (i = 0; i < 16; i++) {
			if (i % 4 == 0)
				rnd = arc4random();
			snprintf(data + 2 * i, sizeof data - 2 * i, "%02x",
			    rnd & 0xff);
			rnd >>= 8;
		}
	}
}

static void
ssh_init_forwarding(void)
{
	int success = 0;
	int i;

	/* Initiate local TCP/IP port forwardings. */
	for (i = 0; i < options.num_local_forwards; i++) {
		debug("Connections to local port %d forwarded to remote address %.200s:%d",
		    options.local_forwards[i].port,
		    options.local_forwards[i].host,
		    options.local_forwards[i].host_port);
		success += channel_setup_local_fwd_listener(
		    options.local_forwards[i].port,
		    options.local_forwards[i].host,
		    options.local_forwards[i].host_port,
		    options.gateway_ports);
	}
	if (i > 0 && success == 0)
		error("Could not request local forwarding.");

	/* Initiate remote TCP/IP port forwardings. */
	for (i = 0; i < options.num_remote_forwards; i++) {
		debug("Connections to remote port %d forwarded to local address %.200s:%d",
		    options.remote_forwards[i].port,
		    options.remote_forwards[i].host,
		    options.remote_forwards[i].host_port);
		channel_request_remote_forwarding(
		    options.remote_forwards[i].port,
		    options.remote_forwards[i].host,
		    options.remote_forwards[i].host_port);
	}
}

static void
check_agent_present(void)
{
	if (options.forward_agent) {
		/* Clear agent forwarding if we don\'t have an agent. */
		if (!ssh_agent_present())
			options.forward_agent = 0;
	}
}

static int
ssh_session(void)
{
	int type;
	int interactive = 0;
	int have_tty = 0;
	struct winsize ws;
	char *cp;

	/* Enable compression if requested. */
	if (options.compression) {
		debug("Requesting compression at level %d.", options.compression_level);

		if (options.compression_level < 1 || options.compression_level > 9)
			fatal("Compression level must be from 1 (fast) to 9 (slow, best).");

		/* Send the request. */
		packet_start(SSH_CMSG_REQUEST_COMPRESSION);
		packet_put_int(options.compression_level);
		packet_send();
		packet_write_wait();
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS)
			packet_start_compression(options.compression_level);
		else if (type == SSH_SMSG_FAILURE)
			logit("Warning: Remote host refused compression.");
		else
			packet_disconnect("Protocol error waiting for compression response.");
	}
	/* Allocate a pseudo tty if appropriate. */
	if (tty_flag) {
		debug("Requesting pty.");

		/* Start the packet. */
		packet_start(SSH_CMSG_REQUEST_PTY);

		/* Store TERM in the packet.  There is no limit on the
		   length of the string. */
		cp = getenv("TERM");
		if (!cp)
			cp = "";
		packet_put_cstring(cp);

		/* Store window size in the packet. */
		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
			memset(&ws, 0, sizeof(ws));
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);

		/* Store tty modes in the packet. */
		tty_make_modes(fileno(stdin), NULL);

		/* Send the packet, and wait for it to leave. */
		packet_send();
		packet_write_wait();

		/* Read response from the server. */
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS) {
			interactive = 1;
			have_tty = 1;
		} else if (type == SSH_SMSG_FAILURE)
			logit("Warning: Remote host failed or refused to allocate a pseudo tty.");
		else
			packet_disconnect("Protocol error waiting for pty request response.");
	}
	/* Request X11 forwarding if enabled and DISPLAY is set. */
	if (options.forward_x11 && getenv("DISPLAY") != NULL) {
		char *proto, *data;
		/* Get reasonable local authentication information. */
		x11_get_proto(&proto, &data);
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication spoofing.");
		x11_request_forwarding_with_spoofing(0, proto, data);

		/* Read response from the server. */
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS) {
			interactive = 1;
		} else if (type == SSH_SMSG_FAILURE) {
			logit("Warning: Remote host denied X11 forwarding.");
		} else {
			packet_disconnect("Protocol error waiting for X11 forwarding");
		}
	}
	/* Tell the packet module whether this is an interactive session. */
	packet_set_interactive(interactive);

	/* Request authentication agent forwarding if appropriate. */
	check_agent_present();

	if (options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		auth_request_forwarding();

		/* Read response from the server. */
		type = packet_read();
		packet_check_eom();
		if (type != SSH_SMSG_SUCCESS)
			logit("Warning: Remote host denied authentication agent forwarding.");
	}

	/* Initiate port forwardings. */
	ssh_init_forwarding();

	/* If requested, let ssh continue in the background. */
	if (fork_after_authentication_flag)
		if (daemon(1, 1) < 0)
			fatal("daemon() failed: %.200s", strerror(errno));

	/*
	 * If a command was specified on the command line, execute the
	 * command now. Otherwise request the server to start a shell.
	 */
	if (buffer_len(&command) > 0) {
		int len = buffer_len(&command);
		if (len > 900)
			len = 900;
		debug("Sending command: %.*s", len, (u_char *)buffer_ptr(&command));
		packet_start(SSH_CMSG_EXEC_CMD);
		packet_put_string(buffer_ptr(&command), buffer_len(&command));
		packet_send();
		packet_write_wait();
	} else {
		debug("Requesting shell.");
		packet_start(SSH_CMSG_EXEC_SHELL);
		packet_send();
		packet_write_wait();
	}

	/* Enter the interactive session. */
	return client_loop(have_tty, tty_flag ?
	    options.escape_char : SSH_ESCAPECHAR_NONE, 0);
}

static void
ssh_subsystem_reply(int type, u_int32_t seq, void *ctxt)
{
	int id, len;

	id = packet_get_int();
	len = buffer_len(&command);
	if (len > 900)
		len = 900;
	packet_check_eom();
	if (type == SSH2_MSG_CHANNEL_FAILURE)
		fatal("Request for subsystem '%.*s' failed on channel %d",
		    len, (u_char *)buffer_ptr(&command), id);
}

void
client_global_request_reply_fwd(int type, u_int32_t seq, void *ctxt)
{
	int i;

	i = client_global_request_id++;
	if (i >= options.num_remote_forwards)
		return;
	debug("remote forward %s for: listen %d, connect %s:%d",
	    type == SSH2_MSG_REQUEST_SUCCESS ? "success" : "failure",
	    options.remote_forwards[i].port,
	    options.remote_forwards[i].host,
	    options.remote_forwards[i].host_port);
	if (type == SSH2_MSG_REQUEST_FAILURE)
		logit("Warning: remote port forwarding failed for listen port %d",
		    options.remote_forwards[i].port);
}

static void
ssh_control_listener(void)
{
	struct sockaddr_un addr;
	mode_t old_umask;
	int addr_len;

	if (options.control_path == NULL || options.control_master <= 0)
		return;

	memset(&addr, '\0', sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr_len = offsetof(struct sockaddr_un, sun_path) +
	    strlen(options.control_path) + 1;

	if (strlcpy(addr.sun_path, options.control_path,
	    sizeof(addr.sun_path)) >= sizeof(addr.sun_path))
		fatal("ControlPath too long");

	if ((control_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
		fatal("%s socket(): %s\n", __func__, strerror(errno));

	old_umask = umask(0177);
	if (bind(control_fd, (struct sockaddr*)&addr, addr_len) == -1) {
		control_fd = -1;
		if (errno == EINVAL)
			fatal("ControlSocket %s already exists",
			    options.control_path);
		else
			fatal("%s bind(): %s\n", __func__, strerror(errno));
	}
	umask(old_umask);

	if (listen(control_fd, 64) == -1)
		fatal("%s listen(): %s\n", __func__, strerror(errno));

	set_nonblock(control_fd);
}

/* request pty/x11/agent/tcpfwd/shell for channel */
static void
ssh_session2_setup(int id, void *arg)
{
	extern char **environ;

	int interactive = tty_flag;
	if (options.forward_x11 && getenv("DISPLAY") != NULL) {
		char *proto, *data;
		/* Get reasonable local authentication information. */
		x11_get_proto(&proto, &data);
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication spoofing.");
		x11_request_forwarding_with_spoofing(id, proto, data);
		interactive = 1;
		/* XXX wait for reply */
	}

	check_agent_present();
	if (options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		channel_request_start(id, "auth-agent-req@openssh.com", 0);
		packet_send();
	}

	client_session2_setup(id, tty_flag, subsystem_flag, getenv("TERM"),
	    NULL, fileno(stdin), &command, environ, &ssh_subsystem_reply);

	packet_set_interactive(interactive);
}

/* open new channel for a session */
static int
ssh_session2_open(void)
{
	Channel *c;
	int window, packetmax, in, out, err;

	if (stdin_null_flag) {
		in = open(_PATH_DEVNULL, O_RDONLY);
	} else {
		in = dup(STDIN_FILENO);
	}
	out = dup(STDOUT_FILENO);
	err = dup(STDERR_FILENO);

	if (in < 0 || out < 0 || err < 0)
		fatal("dup() in/out/err failed");

	/* enable nonblocking unless tty */
	if (!isatty(in))
		set_nonblock(in);
	if (!isatty(out))
		set_nonblock(out);
	if (!isatty(err))
		set_nonblock(err);

	window = CHAN_SES_WINDOW_DEFAULT;
	packetmax = CHAN_SES_PACKET_DEFAULT;
	if (tty_flag) {
		window >>= 1;
		packetmax >>= 1;
	}
	c = channel_new(
	    "session", SSH_CHANNEL_OPENING, in, out, err,
	    window, packetmax, CHAN_EXTENDED_WRITE,
	    "client-session", /*nonblock*/0);

	debug3("ssh_session2_open: channel_new: %d", c->self);

	channel_send_open(c->self);
	if (!no_shell_flag)
		channel_register_confirm(c->self, ssh_session2_setup, NULL);

	return c->self;
}

static int
ssh_session2(void)
{
	int id = -1;

	/* XXX should be pre-session */
	ssh_init_forwarding();
	ssh_control_listener();

	if (!no_shell_flag || (datafellows & SSH_BUG_DUMMYCHAN))
		id = ssh_session2_open();

	/* If requested, let ssh continue in the background. */
	if (fork_after_authentication_flag)
		if (daemon(1, 1) < 0)
			fatal("daemon() failed: %.200s", strerror(errno));

	return client_loop(tty_flag, tty_flag ?
	    options.escape_char : SSH_ESCAPECHAR_NONE, id);
}

static void
load_public_identity_files(void)
{
	char *filename;
	int i = 0;
	Key *public;
#ifdef SMARTCARD
	Key **keys;

	if (options.smartcard_device != NULL &&
	    options.num_identity_files < SSH_MAX_IDENTITY_FILES &&
	    (keys = sc_get_keys(options.smartcard_device, NULL)) != NULL ) {
		int count = 0;
		for (i = 0; keys[i] != NULL; i++) {
			count++;
			memmove(&options.identity_files[1], &options.identity_files[0],
			    sizeof(char *) * (SSH_MAX_IDENTITY_FILES - 1));
			memmove(&options.identity_keys[1], &options.identity_keys[0],
			    sizeof(Key *) * (SSH_MAX_IDENTITY_FILES - 1));
			options.num_identity_files++;
			options.identity_keys[0] = keys[i];
			options.identity_files[0] = sc_get_key_label(keys[i]);
		}
		if (options.num_identity_files > SSH_MAX_IDENTITY_FILES)
			options.num_identity_files = SSH_MAX_IDENTITY_FILES;
		i = count;
		xfree(keys);
	}
#endif /* SMARTCARD */
	for (; i < options.num_identity_files; i++) {
		filename = tilde_expand_filename(options.identity_files[i],
		    original_real_uid);
		public = key_load_public(filename, NULL);
		debug("identity file %s type %d", filename,
		    public ? public->type : -1);
		xfree(options.identity_files[i]);
		options.identity_files[i] = filename;
		options.identity_keys[i] = public;
	}
}

static void
control_client_sighandler(int signo)
{
	control_client_terminate = signo;
}

static void
control_client_sigrelay(int signo)
{
	if (control_server_pid > 1)
		kill(control_server_pid, signo);
}

static int
env_permitted(char *env)
{
	int i;
	char name[1024], *cp;

	strlcpy(name, env, sizeof(name));
	if ((cp = strchr(name, '=')) == NULL)
		return (0);

	*cp = '\0';

	for (i = 0; i < options.num_send_env; i++)
		if (match_pattern(name, options.send_env[i]))
			return (1);

	return (0);
}

static void
control_client(const char *path)
{
	struct sockaddr_un addr;
	int i, r, sock, exitval, num_env, addr_len;
	Buffer m;
	char *cp;
	extern char **environ;

	memset(&addr, '\0', sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr_len = offsetof(struct sockaddr_un, sun_path) +
	    strlen(path) + 1;

	if (strlcpy(addr.sun_path, path,
	    sizeof(addr.sun_path)) >= sizeof(addr.sun_path))
		fatal("ControlPath too long");

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
		fatal("%s socket(): %s", __func__, strerror(errno));

	if (connect(sock, (struct sockaddr*)&addr, addr_len) == -1)
		fatal("Couldn't connect to %s: %s", path, strerror(errno));

	if ((cp = getenv("TERM")) == NULL)
		cp = "";

	buffer_init(&m);

	/* Get PID of controlee */
	if (ssh_msg_recv(sock, &m) == -1)
		fatal("%s: msg_recv", __func__);
	if (buffer_get_char(&m) != 0)
		fatal("%s: wrong version", __func__);
	/* Connection allowed? */
	if (buffer_get_int(&m) != 1)
		fatal("Connection to master denied");
	control_server_pid = buffer_get_int(&m);

	buffer_clear(&m);
	buffer_put_int(&m, tty_flag);
	buffer_put_int(&m, subsystem_flag);
	buffer_put_cstring(&m, cp);

	buffer_append(&command, "\0", 1);
	buffer_put_cstring(&m, buffer_ptr(&command));

	if (options.num_send_env == 0 || environ == NULL) {
		buffer_put_int(&m, 0);
	} else {
		/* Pass environment */
		num_env = 0;
		for (i = 0; environ[i] != NULL; i++)
			if (env_permitted(environ[i]))
				num_env++; /* Count */

		buffer_put_int(&m, num_env);

		for (i = 0; environ[i] != NULL && num_env >= 0; i++)
			if (env_permitted(environ[i])) {
				num_env--;
				buffer_put_cstring(&m, environ[i]);
			}
	}

	if (ssh_msg_send(sock, /* version */0, &m) == -1)
		fatal("%s: msg_send", __func__);

	mm_send_fd(sock, STDIN_FILENO);
	mm_send_fd(sock, STDOUT_FILENO);
	mm_send_fd(sock, STDERR_FILENO);

	/* Wait for reply, so master has a chance to gather ttymodes */
	buffer_clear(&m);
	if (ssh_msg_recv(sock, &m) == -1)
		fatal("%s: msg_recv", __func__);
	if (buffer_get_char(&m) != 0)
		fatal("%s: master returned error", __func__);
	buffer_free(&m);

	signal(SIGINT, control_client_sighandler);
	signal(SIGTERM, control_client_sighandler);
	signal(SIGWINCH, control_client_sigrelay);

	if (tty_flag)
		enter_raw_mode();

	/* Stick around until the controlee closes the client_fd */
	exitval = 0;
	for (;!control_client_terminate;) {
		r = read(sock, &exitval, sizeof(exitval));
		if (r == 0) {
			debug2("Received EOF from master");
			break;
		}
		if (r > 0)
			debug2("Received exit status from master %d", exitval);
		if (r == -1 && errno != EINTR)
			fatal("%s: read %s", __func__, strerror(errno));
	}

	if (control_client_terminate)
		debug2("Exiting on signal %d", control_client_terminate);

	close(sock);

	leave_raw_mode();

	if (tty_flag && options.log_level != SYSLOG_LEVEL_QUIET)
		fprintf(stderr, "Connection to master closed.\r\n");

	exit(exitval);
}
