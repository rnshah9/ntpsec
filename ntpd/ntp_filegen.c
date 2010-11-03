/*
 * ntp_filegen.c,v 3.12 1994/01/25 19:06:11 kardel Exp
 *
 *  implements file generations support for NTP
 *  logfiles and statistic files
 *
 *
 * Copyright (C) 1992, 1996 by Rainer Pruy
 * Friedrich-Alexander Universitaet Erlangen-Nuernberg, Germany
 *
 * This code may be modified and used freely
 * provided credits remain intact.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_string.h"
#include "ntp_calendar.h"
#include "ntp_filegen.h"
#include "ntp_stdlib.h"

/*
 * NTP is intended to run long periods of time without restart.
 * Thus log and statistic files generated by NTP will grow large.
 *
 * this set of routines provides a central interface 
 * to generating files using file generations
 *
 * the generation of a file is changed according to file generation type
 */


/*
 * redefine this if your system dislikes filename suffixes like
 * X.19910101 or X.1992W50 or ....
 */
#define SUFFIX_SEP '.'

static	void	filegen_open	(FILEGEN *, u_int32, const time_t*);
static	int	valid_fileref	(const char *, const char *);
static	void	filegen_init	(const char *, const char *, FILEGEN *);
#ifdef	DEBUG
static	void	filegen_uninit		(FILEGEN *);
#endif	/* DEBUG */


/*
 * filegen_init
 */

static void
filegen_init(
	const char *prefix  ,
	const char *filename,
	FILEGEN	   *fgp	    )
{
	fgp->fp	      = NULL;
	fgp->prefix   = prefix;		/* Yes, this is TOTALLY lame! */
	fgp->basename = estrdup(filename);
	fgp->id_lo    = 0;
	fgp->id_hi    = 0;
	fgp->type     = FILEGEN_DAY;
	fgp->flag     = FGEN_FLAG_LINK; /* not yet enabled !!*/
}


/*
 * filegen_uninit - free memory allocated by filegen_init
 */
#ifdef DEBUG
static void
filegen_uninit(
	FILEGEN *fgp
	)
{
	free(fgp->basename);
}
#endif


/*
 * open a file generation according to the current settings of gen
 * will also provide a link to basename if requested to do so
 */

static void
filegen_open(
	FILEGEN	     *gen  ,
	u_int32	      stamp,
	const time_t *pivot)
{
	char *savename;	/* temp store for name collision handling */
	char *fullname;	/* name with any designation extension */
	char *filename;	/* name without designation extension */
	char *suffix;	/* where to print suffix extension */
	u_int len, suflen;
	FILE *fp;
	struct calendar cal;
	struct isodate	iso;

	/* get basic filename in buffer, leave room for extensions */
	len = strlen(gen->prefix) + strlen(gen->basename) + 65;
	filename = emalloc(len);
	fullname = emalloc(len);
	savename = NULL;
	snprintf(filename, len, "%s%s", gen->prefix, gen->basename);
	strncpy(fullname, filename, len);
	fullname[len-1] = '\0'; /* prepare overflow detection */

	/* where to place suffix */
	suflen = strlen(fullname);
	suffix = fullname + suflen;
	suflen = len - suflen;

	switch (gen->type) {

	default:
		msyslog(LOG_ERR, 
			"unsupported file generations type %d for "
			"\"%s\" - reverting to FILEGEN_NONE",
			gen->type, filename);
		gen->type = FILEGEN_NONE;
		/* fall through to FILEGEN_NONE */

	case FILEGEN_NONE:
		strncpy(fullname, filename, len);
		break;

	case FILEGEN_PID:
		gen->id_lo = getpid();
		gen->id_hi = 0;
		snprintf(suffix, suflen, "%c#%ld",
			 SUFFIX_SEP, gen->id_lo);
		break;

	case FILEGEN_DAY:
		/*
		 * You can argue here in favor of using MJD, but I
		 * would assume it to be easier for humans to interpret
		 * dates in a format they are used to in everyday life.
		 */
		ntpcal_ntp_to_date(&cal, stamp, pivot);
		snprintf(suffix, suflen, "%c%04d%02d%02d",
			 SUFFIX_SEP, cal.year, cal.month, cal.monthday);
		cal.hour = cal.minute = cal.second = 0;
		gen->id_lo = ntpcal_date_to_ntp(&cal); 
		gen->id_hi = (u_int32)(gen->id_lo + SECSPERDAY);
		break;

	case FILEGEN_WEEK:
		isocal_ntp_to_date(&iso, stamp, pivot);
		snprintf(suffix, suflen, "%c%04dw%02d",
			 SUFFIX_SEP, iso.year, iso.week);
		iso.hour = iso.minute = iso.second = 0;
		iso.weekday = 1;
		gen->id_lo = isocal_date_to_ntp(&iso);
		gen->id_hi = (u_int32)(gen->id_lo + 7*SECSPERDAY);
		break;

	case FILEGEN_MONTH:
		ntpcal_ntp_to_date(&cal, stamp, pivot);
		snprintf(suffix, suflen, "%c%04d%02d",
			 SUFFIX_SEP, cal.year, cal.month);
		cal.hour = cal.minute = cal.second = 0;
		cal.monthday = 1;
		gen->id_lo = ntpcal_date_to_ntp(&cal); 
		cal.month++;
		gen->id_hi = ntpcal_date_to_ntp(&cal); 
		break;

	case FILEGEN_YEAR:
		ntpcal_ntp_to_date(&cal, stamp, pivot);
		snprintf(suffix, suflen, "%c%04d",
			 SUFFIX_SEP, cal.year);
		cal.hour = cal.minute = cal.second = 0;
		cal.month = cal.monthday = 1;
		gen->id_lo = ntpcal_date_to_ntp(&cal); 
		cal.year++;
		gen->id_hi = ntpcal_date_to_ntp(&cal); 
		break;

	case FILEGEN_AGE:
		gen->id_lo = current_time - (current_time % SECSPERDAY);
		gen->id_hi = gen->id_lo + SECSPERDAY;
		snprintf(suffix, suflen, "%ca%08ld",
			 SUFFIX_SEP, gen->id_lo);
	}
  
	/* check possible truncation */
	if (fullname[len-1]) {
		fullname[len-1] = '\0';
		DPRINTF(1, ("logfile name truncated: \"%s\"",
			    fullname));
		msyslog(LOG_ERR, "logfile name truncated: \"%s\"",
			fullname);
	}

	if (FILEGEN_NONE != gen->type) {
		/*
		 * check for existence of a file with name 'basename'
		 * as we disallow such a file
		 * if FGEN_FLAG_LINK is set create a link
		 */
		struct stat stats;
		/*
		 * try to resolve name collisions
		 */
		static u_long conflicts = 0;

#ifndef	S_ISREG
#define	S_ISREG(mode)	(((mode) & S_IFREG) == S_IFREG)
#endif
		if (stat(filename, &stats) == 0) {
			/* Hm, file exists... */
			if (S_ISREG(stats.st_mode)) {
				if (stats.st_nlink <= 1)	{
					/*
					 * Oh, it is not linked - try to save it
					 */
					savename = emalloc(len);
					snprintf(savename, len,
						"%s%c%dC%lu",
						filename, SUFFIX_SEP,
						(int)getpid(), conflicts++);

					if (rename(filename, savename) != 0)
						msyslog(LOG_ERR,
							"couldn't save %s: %m",
							filename);
					free(savename);
				} else {
					/*
					 * there is at least a second link to
					 * this file.
					 * just remove the conflicting one
					 */
					if (
#if !defined(VMS)
						unlink(filename) != 0
#else
						delete(filename) != 0
#endif
						)
						msyslog(LOG_ERR, 
							"couldn't unlink %s: %m",
							filename);
				}
			} else {
				/*
				 * Ehh? Not a regular file ?? strange !!!!
				 */
				msyslog(LOG_ERR, 
					"expected regular file for %s "
					"(found mode 0%lo)",
					filename,
					(unsigned long)stats.st_mode);
			}
		} else {
			/*
			 * stat(..) failed, but it is absolutely correct for
			 * 'basename' not to exist
			 */
			if (ENOENT != errno)
				msyslog(LOG_ERR, "stat(%s) failed: %m",
						 filename);
		}
	}

	/*
	 * now, try to open new file generation...
	 */
	fp = fopen(fullname, "a");
  
	DPRINTF(4, ("opening filegen (type=%d/stamp=%u) \"%s\"\n",
		    gen->type, stamp, fullname));

	if (NULL == fp)	{
		/* open failed -- keep previous state
		 *
		 * If the file was open before keep the previous generation.
		 * This will cause output to end up in the 'wrong' file,
		 * but I think this is still better than losing output
		 *
		 * ignore errors due to missing directories
		 */

		if (ENOENT != errno)
			msyslog(LOG_ERR, "can't open %s: %m", fullname);
	} else {
		if (NULL != gen->fp) {
			fclose(gen->fp);
			gen->fp = NULL;
		}
		gen->fp = fp;

		if (gen->flag & FGEN_FLAG_LINK) {
			/*
			 * need to link file to basename
			 * have to use hardlink for now as I want to allow
			 * gen->basename spanning directory levels
			 * this would make it more complex to get the correct
			 * fullname for symlink
			 *
			 * Ok, it would just mean taking the part following
			 * the last '/' in the name.... Should add it later....
			 */

			/* Windows NT does not support file links -Greg Schueman 1/18/97 */

#if defined SYS_WINNT || defined SYS_VXWORKS
			SetLastError(0); /* On WinNT, don't support FGEN_FLAG_LINK */
#elif defined(VMS)
			errno = 0; /* On VMS, don't support FGEN_FLAG_LINK */
#else  /* not (VMS) / VXWORKS / WINNT ; DO THE LINK) */
			if (link(fullname, filename) != 0)
				if (EEXIST != errno)
					msyslog(LOG_ERR, 
						"can't link(%s, %s): %m",
						fullname, filename);
#endif /* SYS_WINNT || VXWORKS */
		}		/* flags & FGEN_FLAG_LINK */
	}			/* else fp == NULL */
	
	free(filename);
	free(fullname);
	return;
}

/*
 * this function sets up gen->fp to point to the correct
 * generation of the file for the time specified by 'now'
 *
 * 'now' usually is interpreted as second part of a l_fp as is in the cal...
 * library routines
 */

void
filegen_setup(
	FILEGEN *gen,
	u_int32	 now
	)
{
	int    current = 0;
	u_long new_gen = ~ (u_long) 0;
	time_t pivot   = time(NULL);

	if (!(gen->flag & FGEN_FLAG_ENABLED)) {
		if (NULL != gen->fp) {
			fclose(gen->fp);
			gen->fp = NULL;
		}
		return;
	}

	switch (gen->type) {

	default:
	case FILEGEN_NONE:
		current = 1;
		break;

	case FILEGEN_PID:
		current = (gen->id_lo == getpid());
		break;

	case FILEGEN_AGE:
		if (gen->id_lo <= gen->id_hi)
			current = (gen->id_lo <= current_time) &&
				  (gen->id_hi > current_time);
		else
			current = (gen->id_lo <= current_time) ||
				  (gen->id_hi > current_time);
		break;

	case FILEGEN_DAY:
	case FILEGEN_WEEK:
	case FILEGEN_MONTH:
	case FILEGEN_YEAR:
		if (gen->id_lo <= gen->id_hi)
			current = (gen->id_lo <= now) &&
				  (gen->id_hi > now);
		else
			current = (gen->id_lo <= now) ||
				  (gen->id_hi > now);
		break;
	}
	/*
	 * try to open file if not yet open
	 * reopen new file generation file on change of generation id
	 */
	if (NULL == gen->fp || !current) {
		DPRINTF(1, ("filegen  %0x %u\n", gen->type, now));
		filegen_open(gen, now, &pivot);
	}
}


/*
 * change settings for filegen files
 */
void
filegen_config(
	FILEGEN *	gen,
	const char *	basename,
	u_int		type,
	u_int		flag
	)
{
	int file_existed = 0;

	/*
	 * if nothing would be changed...
	 */
	if ((strcmp(basename, gen->basename) == 0) && type == gen->type
	    && flag == gen->flag)
		return;
  
	/*
	 * validate parameters
	 */
	if (!valid_fileref(gen->prefix, basename))
		return;
  
	if (NULL != gen->fp) {
		fclose(gen->fp);
		gen->fp = NULL;
		file_existed = 1;
	}

	DPRINTF(3, ("configuring filegen:\n"
		    "\tprefix:\t%s\n"
		    "\tbasename:\t%s -> %s\n"
		    "\ttype:\t%d -> %d\n"
		    "\tflag: %x -> %x\n",
		    gen->prefix,
		    gen->basename, basename,
		    gen->type, type,
		    gen->flag, flag));

	if (strcmp(gen->basename, basename) != 0) {
		free(gen->basename);
		gen->basename = estrdup(basename);
	}
	gen->type = (u_char)type;
	gen->flag = (u_char)flag;

	/*
	 * make filegen use the new settings
	 * special action is only required when a generation file
	 * is currently open
	 * otherwise the new settings will be used anyway at the next open
	 */
	if (file_existed) {
		l_fp now;

		get_systime(&now);
		filegen_setup(gen, now.l_ui);
	}
}


/*
 * check whether concatenating prefix and basename
 * yields a legal filename
 */
static int
valid_fileref(
	const char *	prefix,
	const char *	basename
	)
{
	/*
	 * prefix cannot be changed dynamically
	 * (within the context of filegen)
	 * so just reject basenames containing '..'
	 *
	 * ASSUMPTION:
	 *		file system parts 'below' prefix may be
	 *		specified without infringement of security
	 *
	 *		restricting prefix to legal values
	 *		has to be ensured by other means
	 * (however, it would be possible to perform some checks here...)
	 */
	register const char *p = basename;
  
	/*
	 * Just to catch, dumb errors opening up the world...
	 */
	if (NULL == prefix || '\0' == *prefix)
		return 0;

	if (NULL == basename)
		return 0;
  
	for (p = basename; p; p = strchr(p, DIR_SEP)) {
		if ('.' == p[0] && '.' == p[1] 
		    && ('\0' == p[2] || DIR_SEP == p[2]))
			return 0;
	}
  
	return 1;
}


/*
 * filegen registry
 */

static struct filegen_entry {
	char *			name;
	FILEGEN *		filegen;
	struct filegen_entry *	next;
} *filegen_registry = NULL;


FILEGEN *
filegen_get(
	const char *	name
	)
{
	struct filegen_entry *f = filegen_registry;

	while (f) {
		if (f->name == name || strcmp(name, f->name) == 0) {
			DPRINTF(4, ("filegen_get(%s) = %p\n",
				    name, f->filegen));
			return f->filegen;
		}
		f = f->next;
	}
	DPRINTF(4, ("filegen_get(%s) = NULL\n", name));
	return NULL;
}


void
filegen_register(
	const char *	prefix,
	const char *	name,
	FILEGEN *	filegen
	)
{
	struct filegen_entry **ppfe;

	DPRINTF(4, ("filegen_register(%s, %p)\n", name, filegen));

	filegen_init(prefix, name, filegen);

	ppfe = &filegen_registry;
	while (NULL != *ppfe) {
		if ((*ppfe)->name == name 
		    || !strcmp((*ppfe)->name, name)) {

			DPRINTF(5, ("replacing filegen %p\n",
				    (*ppfe)->filegen));

			(*ppfe)->filegen = filegen;
			return;
		}
		ppfe = &((*ppfe)->next);
	}

	*ppfe = emalloc(sizeof **ppfe);

	(*ppfe)->next = NULL;
	(*ppfe)->name = estrdup(name);
	(*ppfe)->filegen = filegen;

	DPRINTF(6, ("adding new filegen\n"));
	
	return;
}


/*
 * filegen_unregister frees memory allocated by filegen_register for
 * name.
 */
#ifdef DEBUG
void
filegen_unregister(
	char *name
	)
{
	struct filegen_entry **	ppfe;
	struct filegen_entry *	pfe;
	FILEGEN *		fg;
			
	DPRINTF(4, ("filegen_unregister(%s)\n", name));

	ppfe = &filegen_registry;

	while (NULL != *ppfe) {
		if ((*ppfe)->name == name
		    || !strcmp((*ppfe)->name, name)) {
			pfe = *ppfe;
			*ppfe = (*ppfe)->next;
			fg = pfe->filegen;
			free(pfe->name);
			free(pfe);
			filegen_uninit(fg);
			break;
		}
		ppfe = &((*ppfe)->next);
	}
}	
#endif	/* DEBUG */
