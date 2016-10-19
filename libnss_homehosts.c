
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


#define ALIGN(idx) do { \
  if (idx % sizeof(void*)) \
    idx += (sizeof(void*) - idx % sizeof(void*)); /* Align on 32 bit boundary */ \
} while(0)


int parseIpStr(const char *str, struct ipaddr *addr)
{
	/* Convert string to IPv4/v6 address, or fail */
	/* Return: 1 on success */
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
	return ok;
}

void* ipaddr_get_binary_addr(struct ipaddr *addr)
{
	if(addr->af == AF_INET) return &(addr->ip4.s_addr);
	if(addr->af == AF_INET6) return &(addr->ip6.__in6_u);
	return NULL;
}

#ifdef DEBUG
void printbuffer(unsigned char* buf, size_t len)
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

enum nss_status homehosts_gethostbyname_r(
	const char *query_name,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop,
	int query_af)
{
	size_t idx, ridx, addrstart;		// cursors in buffer space
	struct ipaddr address;
	FILE *fh;
	long aliases_offset;
	char homehosts_file[PATH_MAX+1];
	char ipbuf[INET6_ADDRSTRLEN];
	char namebuf[_POSIX_HOST_NAME_MAX+1];
	char nlbuf[2];		// holds a newline char
	char *c;
	int cnt, acnt, tokens;
	int store_aliases_phase;
	
	memset(buffer, ' ', buflen);
	#ifdef DEBUG
	warnx("host.conf: inited = %u, flags = %u, multi = %u", _res_hconf.initialized, _res_hconf.flags, (_res_hconf.flags & HCONF_FLAG_MULTI)!=0);
	#endif
	
	/* Open hosts file */
	
	cnt = snprintf(homehosts_file, PATH_MAX, "%s/.hosts", getenv("HOME"));
	if(cnt >= PATH_MAX) goto soft_error;
	fh = fopen(homehosts_file, "r");
	if(fh == NULL) goto soft_error;
	
	/* Copy requested name to canonical hostname */
	
	idx = 0;
	ridx = buflen;		// first byte occupied at the end of buffer
	strcpy(buffer+idx, query_name);
	result->h_name = buffer+idx;
	idx += strlen(query_name)+1;
	ALIGN(idx);
	addrstart = idx;
	
	result->h_addrtype = query_af;
	result->h_length = (query_af == AF_INET6) ? sizeof(struct in6_addr) : sizeof(struct in_addr);
	
	/* Read hosts file */
	
	cnt = 0;	// Count resulting addresses
	acnt = 0;	// Count resulting alias names
	while(!feof(fh))
	{
		if(fscanf(fh, "%s", &ipbuf) == 1)
		{
			if(ipbuf[0] == '#')
			{
				/* Seek to the next line */
				fscanf(fh, "%*[^\n]%*[\n]");
				continue;
			}
			
			store_aliases_phase = 0;
			aliases_offset = ftell(fh);
			
			read_hostname:
			tokens = fscanf(fh, "%s%1[\n]", namebuf, nlbuf);
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
					fscanf(fh, "%*[^\n]%*[\n]");
				}
				
				if(store_aliases_phase)
				{
					if(strcasecmp(namebuf, query_name)!=0)
					{
						acnt++;
						if(idx + strlen(namebuf)+1 /* trailing NUL byte */ + (acnt+1) * sizeof(char*) /* pointers to alias names */ > ridx-1)
						{
							fclose(fh);
							goto buffer_error;
						}
						strcpy(buffer+ridx-strlen(namebuf)-1, namebuf);
						ridx += -strlen(namebuf)-1;
					}
				}
				else
				{
					if(strcasecmp(namebuf, query_name)==0)
					{
						/* hostname matches */
						if(parseIpStr(ipbuf, &address) == 1 && address.af == query_af)
						{
							/* hostname matches and ip address is valid */
							cnt++;
							if(idx + result->h_length + (cnt+1) * sizeof(char*) > ridx-1)
							{
								fclose(fh);
								goto buffer_error;
							}
							
							memcpy(buffer+idx, ipaddr_get_binary_addr(&address), result->h_length);
							idx += result->h_length;
							
							/* Treat other hostnames in this line as aliases */
							store_aliases_phase = 1;
							fseek(fh, aliases_offset, 0);
							goto read_hostname;
						}
					}
				}
			}
			
			if(tokens != 1)
			{
				/* Encountered newline */
				if(cnt > 0 && (_res_hconf.flags & HCONF_FLAG_MULTI)==0)
					break;
				continue;
			}
			
			goto read_hostname;
		}
	}
	fclose(fh);	
	
	if(cnt == 0)
	{
		//*errnop = ;
		*h_errnop = NO_ADDRESS;
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
	
	/* Store pointers to aliases */
	
	idx = addrstart + n * result->h_length + (n+1) * sizeof(char*);
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
	printbuffer(buffer, buflen);
	#endif
	
	return NSS_STATUS_SUCCESS;
	
	
	buffer_error:
	*errnop = ERANGE;
	*h_errnop = NO_RECOVERY;
	return NSS_STATUS_TRYAGAIN;
	
	soft_error:
	*errnop = EAGAIN;
	*h_errnop = NO_RECOVERY;
	return NSS_STATUS_TRYAGAIN;
}

enum nss_status _nss_homehosts_gethostbyname_r(
	const char *name,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	enum nss_status found_ipv4;
	found_ipv4 = homehosts_gethostbyname_r(name, result, buffer, buflen, errnop, h_errnop, AF_INET);
	if(found_ipv4 == NSS_STATUS_NOTFOUND)
		return homehosts_gethostbyname_r(name, result, buffer, buflen, errnop, h_errnop, AF_INET6);
	return found_ipv4;
}

enum nss_status _nss_homehosts_gethostbyname2_r(
	const char *name,
	int af,
	struct hostent * result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	if (af != AF_INET && af != AF_INET6)
	{
		*errnop = EAFNOSUPPORT;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}
	else
	{
		return homehosts_gethostbyname_r(name, result, buffer, buflen, errnop, h_errnop, af);
	}
}

/*
enum nss_status _nss_homehosts_gethostbyaddr_r(
	const void *address,
	socklen_t len,
	int af,
	struct hostent * ret,
	char *buffer,
	size_t buflen,
	struct hostent **result,
	int *h_errnop)
{
}
*/

struct hostent * _nss_homehosts_gethostbyaddr(
	const void *addr,
	socklen_t len,
	int af)
{
	struct hostent result;
	return NULL;
}

