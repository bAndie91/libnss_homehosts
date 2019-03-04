
#include <arpa/inet.h>
#include <nss.h>
#include <netdb.h>
#include <errno.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "res_hconf.h"


struct ipaddr {
	int af;
	struct in_addr ip4;
	struct in6_addr ip6;
};

typedef int bool;
#define TRUE 1
#define FALSE 0

#define ALIGN(idx) do { \
  if (idx % sizeof(void*)) \
    idx += (sizeof(void*) - idx % sizeof(void*)); /* Align on 32 bit boundary */ \
} while(0)

#define AFLEN(af) (((af) == AF_INET6) ? sizeof(struct in6_addr) : sizeof(struct in_addr))

#define OPEN_HOME_HOSTS(fh) do { \
	fh = NULL; \
	cnt = -1; \
	c = getenv("XDG_CONFIG_HOME"); \
	if(c != NULL) \
		cnt = snprintf(homehosts_file, PATH_MAX, "%s/hosts", c); \
	c = getenv("HOME"); \
	if(cnt < 0) \
		cnt = snprintf(homehosts_file, PATH_MAX, "%s/.config/hosts", c); \
	if(cnt >= 0 && cnt < PATH_MAX) \
		fh = fopen(homehosts_file, "r"); \
	\
	if(fh == NULL) \
	{ \
		cnt = snprintf(homehosts_file, PATH_MAX, "%s/.hosts", c); \
		if(cnt >= PATH_MAX) goto soft_error; \
		fh = fopen(homehosts_file, "r"); \
		if(fh == NULL) goto soft_error; \
	} \
} while(0)


bool parseIpStr(const char *str, struct ipaddr *addr)
{
	/* Convert string to IPv4/v6 address, or fail */
	int ok;
	
	addr->af = AF_INET;
	ok = inet_pton(AF_INET, str, &(addr->ip4));
	if(ok == -1) perror("inet_pton");
	if(ok != 1)
	{
		addr->af = AF_INET6;
		ok = inet_pton(AF_INET6, str, &(addr->ip6));
		if(ok == -1) perror("inet_pton");
	}
	return ok == 1 ? TRUE : FALSE;
}

void* ipaddr_get_binary_addr(struct ipaddr *addr)
{
	if(addr->af == AF_INET) return &(addr->ip4.s_addr);
	if(addr->af == AF_INET6) return &(addr->ip6.__in6_u);
	return NULL;
}

int seek_line(FILE* fh)
{
	/* Seeks to the beginning of next non-empty line on a file. */
	return fscanf(fh, "%*[^\n]%*[\n]");
}
int fscanfw(FILE* fh, const char* ffmt, char* buf)
{
	int tokens;
	char nlbuf[2];		// holds a newline char
	tokens = fscanf(fh, ffmt, buf, nlbuf);
	if(tokens == 1)
	{
		/* eat non-newline whitespaces */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
		fscanf(fh, "%*[ \f\r\t\v]");
#pragma GCC diagnostic pop
		/* scan for newline, if found then treat like it was found at first fscanf() */
		if(fscanf(fh, "%1[\n]", nlbuf) == 1)
			tokens = 2;
	}
	return tokens;
}


#ifdef WITH_DUMPBUFFER
void dumpbuffer(unsigned char* buf, size_t len)
{
	unsigned char* p = buf;
	while(p - buf < len)
	{
		fprintf(stderr, "%02X %c ", p[0], isprint(p[0])?p[0]:'.');
		if(((p - buf) % 16) == 15) fprintf(stderr, "\n");
		else if(((p - buf) % 8) == 7) fprintf(stderr, "| ");
		p++;
	}
}
#endif

enum nss_status homehosts_gethostent_r(
	const char *query_name,
	const void *query_addr,
	FILE* fh,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	struct hostent ** result_p,
	int *h_errnop,
	int query_af)
{
	size_t idx, ridx, addrstart;		// cursors in buffer space
	struct ipaddr address;
	long aliases_offset;
	char homehosts_file[PATH_MAX+1];
	char ipbuf[INET6_ADDRSTRLEN+1];
	char namebuf[_POSIX_HOST_NAME_MAX+1];
	char ffmt_ip[10];	// fscanf format string
	char ffmt_name[12];	// fscanf format string
	char *c;
	int cnt, acnt, tokens;
	bool store_aliases_phase, ipaddr_found = FALSE;
	bool managed_fh = FALSE;
	

	snprintf(ffmt_ip, sizeof(ffmt_ip), "%%%us%%1[\n]", INET6_ADDRSTRLEN); // generates: %46s%1[\n]
	snprintf(ffmt_name, sizeof(ffmt_name), "%%%us%%1[\n]", _POSIX_HOST_NAME_MAX); // generates: %255s%1[\n]
	
	#ifdef DEBUG
	warnx("%s(%s, ...)", __func__, query_name);
	warnx("host.conf: inited = %u, flags = %u, multi = %u", _res_hconf.initialized, _res_hconf.flags, (_res_hconf.flags & HCONF_FLAG_MULTI)!=0);
	memset(buffer, ' ', buflen);
	#endif
	
	/* Open hosts file */
	
	if(fh == NULL)
	{
		managed_fh = TRUE;
		OPEN_HOME_HOSTS(fh);
	}
	
	/* Copy requested name to canonical hostname */
	
	idx = 0;
	ridx = buflen;		// first byte occupied at the end of buffer
	result->h_name = NULL;
	if(query_name != NULL)
	{
		strcpy(buffer+idx, query_name);
		result->h_name = buffer+idx;
		idx += strlen(query_name)+1;
		ALIGN(idx);
	}
	addrstart = idx;
	
	if(query_af)
	{
		result->h_addrtype = query_af;
		result->h_length = AFLEN(query_af);
	}
	
	/* Read hosts file */
	
	cnt = 0;	// Count resulting addresses
	acnt = 0;	// Count resulting alias names
	while(!feof(fh))
	{
		tokens = fscanfw(fh, ffmt_ip /* "%46s%1[\n]" */, ipbuf);
		if(tokens > 0)
		{
			if(ipbuf[0] == '#')
			{
				seek_line(fh);
				continue;
			}
			
			store_aliases_phase = FALSE;
			aliases_offset = ftell(fh);
			
			if(query_addr != NULL || query_name == NULL)
			{
				/* if address was asked OR neither name nor address were asked */
				if( parseIpStr(ipbuf, &address)
				   && (query_af == 0 || address.af == query_af)
				   && (query_addr == NULL || memcmp(ipaddr_get_binary_addr(&address), query_addr /* TODO: use struct members */, result->h_length)==0))
				{
					if(query_af == 0)
					{
						result->h_addrtype = address.af;
						result->h_length = AFLEN(address.af);
					}
					
					ipaddr_found = TRUE;
					store_aliases_phase = TRUE;
					cnt++;
					memcpy(buffer+idx, ipaddr_get_binary_addr(&address), result->h_length);
					idx += result->h_length;
				}
				else
				{
					if(tokens <= 1)
					{
						seek_line(fh);
					}
					continue;
				}
			}
			
			if(tokens > 1)
			{
				/* Encountered a newline right after the IP address */
				if(ipaddr_found && result->h_name == NULL)
				{
					/* Store an empty hostname */
					*(char*)(buffer+idx) = '\0';
					result->h_name = buffer+idx;
					idx += 1;
					ALIGN(idx);
				}
				if(ipaddr_found)
					/* jump out if address was asked and it is found */
					break;
				continue;
			}
			
			read_hostname:
			tokens = fscanfw(fh, ffmt_name /* "%255s%1[\n]" */, namebuf);
			if(tokens > 0)
			{
				#ifdef DEBUG
				warnx("alias phase %d, name '%s'", store_aliases_phase, namebuf);
				#endif
				c = strchr(namebuf, '#');
				if(c != NULL)
				{
					/* Strip comment */
					*c = '\0';
					/* Treat as we saw newline */
					tokens = 2;
					/* Seek to the next line */
					seek_line(fh);
				}
				
				if(store_aliases_phase)
				{
					if(query_name == NULL || strcasecmp(namebuf, query_name)!=0)
					{
						if(result->h_name == NULL)
						{
							/* Save 1st hostname as canonical name */
							strcpy(buffer+idx, namebuf);
							result->h_name = buffer+idx;
							idx += strlen(namebuf)+1;
							ALIGN(idx);
						}
						else
						{
							acnt++;
							if(idx + strlen(namebuf)+1 /* trailing NUL byte */ + (acnt+1) * sizeof(char*) /* pointers to alias names */ > ridx-1)
							{
								if(managed_fh) fclose(fh);
								goto buffer_error;
							}
							/* Store this alias name at the end of buffer */
							strcpy(buffer+ridx-strlen(namebuf)-1, namebuf);
							ridx += -strlen(namebuf)-1;
						}
					}
				}
				else
				{
					if(strcasecmp(namebuf, query_name)==0)
					{
						/* hostname matches */
						if(parseIpStr(ipbuf, &address) && address.af == query_af)
						{
							/* hostname matches and ip address is valid */
							cnt++;
							if(idx + result->h_length + (cnt+1) * sizeof(char*) > ridx-1)
							{
								if(managed_fh) fclose(fh);
								goto buffer_error;
							}
							
							memcpy(buffer+idx, ipaddr_get_binary_addr(&address), result->h_length);
							idx += result->h_length;
							
							/* Treat other hostnames in this line as aliases */
							store_aliases_phase = TRUE;
							fseek(fh, aliases_offset, 0);
							goto read_hostname;
						}
					}
				}
			}
			
			if(tokens != 1)
			{
				/* Encountered a newline */
				if(cnt > 0 && (ipaddr_found || (_res_hconf.flags & HCONF_FLAG_MULTI)==0))
				{
					/* Do not continue line reading,
					   because either address is found or
					   hostname is found and 'multi off' is in host.conf
					*/
					break;
				}
				continue;
			}
			
			goto read_hostname;
		}
	}
	if(managed_fh) fclose(fh);	
	
	if(cnt == 0)
	{
		*h_errnop = NO_ADDRESS;
		*result_p = NULL;
		return NSS_STATUS_NOTFOUND;
	}
	
	/* Store pointers to addresses */
	
	result->h_addr_list = (char**)(buffer + idx);
	int n = 0;
	for(; n < cnt; n++)
	{
		result->h_addr_list[n] = (char*)(buffer + addrstart + n * result->h_length);
	}
	result->h_addr_list[n] = NULL;
	idx += (n+1) * sizeof(char*);
	
	/* Store pointers to aliases */
	
	result->h_aliases = (char**)(buffer + idx);
	n = 0;
	for(; n < acnt; n++)
	{
		char* alias = (char*)(buffer + ridx);
		#ifdef DEBUG
		warnx("acnt %d, alias '%s'", acnt, alias);
		#endif
		result->h_aliases[n] = alias;
		ridx += strlen(alias) + 1;
	}
	result->h_aliases[n] = NULL;
	
	#ifdef DEBUG
	warnx("h_name -> %u\nh_aliases -> %u\nh_addrtype = %u\nh_length = %u\nh_addr_list -> %u", (void*)result->h_name - (void*)buffer, (void*)result->h_aliases - (void*)buffer, result->h_addrtype, result->h_length, (void*)result->h_addr_list - (void*)buffer);
	dumpbuffer(buffer, buflen);
	#endif
	
	*result_p = result;
	return NSS_STATUS_SUCCESS;
	
	
	buffer_error:
	*h_errnop = ERANGE;
	*result_p = NULL;
	return NSS_STATUS_TRYAGAIN;
	
	soft_error:
	*h_errnop = EAGAIN;
	*result_p = NULL;
	return NSS_STATUS_TRYAGAIN;
}

enum nss_status _nss_homehosts_gethostbyname_r(
	const char *name,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	struct hostent ** result_p,
	int *h_errnop)
{
	enum nss_status found_ipv6;
	found_ipv6 = homehosts_gethostent_r(name, NULL, NULL, result, buffer, buflen, result_p, h_errnop, AF_INET6);
	if(found_ipv6 == NSS_STATUS_NOTFOUND)
		return homehosts_gethostent_r(name, NULL, NULL, result, buffer, buflen, result_p, h_errnop, AF_INET);
	return found_ipv6;
}

enum nss_status _nss_homehosts_gethostbyname2_r(
	const char *name,
	int af,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	struct hostent ** result_p,
	int *h_errnop)
{
	if(af != AF_INET && af != AF_INET6)
	{
		*h_errnop = EAFNOSUPPORT;
		*result_p = NULL;
		return NSS_STATUS_UNAVAIL;
	}
	else
	{
		return homehosts_gethostent_r(name, NULL, NULL, result, buffer, buflen, result_p, h_errnop, af);
	}
}

enum nss_status _nss_homehosts_gethostbyaddr_r(
	const void *address,
	socklen_t len,
	int af,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	struct hostent ** result_p,
	int *h_errnop)
{
	return homehosts_gethostent_r(NULL, address, NULL, result, buffer, buflen, result_p, h_errnop, af);
}


static FILE* sethost_fh;

enum nss_status _nss_homehosts_sethostent(void)
{
	char homehosts_file[PATH_MAX+1];
	int cnt;
	char *c;
	
	OPEN_HOME_HOSTS(sethost_fh);
	return NSS_STATUS_SUCCESS;
	
	soft_error:
	sethost_fh = NULL;
	return NSS_STATUS_TRYAGAIN;
}

enum nss_status _nss_homehosts_gethostent_r(
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop)
{
	if(sethost_fh == NULL)
	{
		*errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}
	
	struct hostent** result_p = &result;
	int* h_errnop = errnop;
	return homehosts_gethostent_r(NULL, NULL, sethost_fh, result, buffer, buflen, result_p, h_errnop, 0);
}

enum nss_status _nss_homehosts_endhostent(void)
{
	if(sethost_fh == NULL)
	{
		return NSS_STATUS_UNAVAIL;
	}
	fclose(sethost_fh);
	return NSS_STATUS_SUCCESS;
}

