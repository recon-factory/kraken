#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "hosts.h"
#include "host_manager.h"
#include "logging.h"
#include "whois_lookup.h"
#include "network_addr.h"

int init_single_host(single_host_info *c_host) {
	memset(c_host, 0, sizeof(struct single_host_info));
	c_host->aliases = NULL;
	c_host->whois_data = NULL;
	return 0;
}

int destroy_single_host(single_host_info *c_host) {
	if (c_host->aliases) {
		free(c_host->aliases);
	}
	c_host->aliases = NULL;
	c_host->n_aliases = 0;
	return 0;
}

int init_host_manager(host_manager *c_host_manager) {
	c_host_manager->hosts = malloc(sizeof(struct single_host_info) * HOST_CAPACITY_INCREMENT_SIZE);
	if (c_host_manager->hosts == NULL) {
		return 1;
	}
	memset(c_host_manager->lw_domain, '\0', DNS_MAX_FQDN_LENGTH + 1);
	c_host_manager->known_hosts = 0;
	c_host_manager->current_capacity = HOST_CAPACITY_INCREMENT_SIZE;
	memset(c_host_manager->hosts, 0, (sizeof(struct single_host_info) * HOST_CAPACITY_INCREMENT_SIZE));
	
	c_host_manager->whois_records = malloc(sizeof(struct whois_record) * WHOIS_CAPACITY_INCREMENT_SIZE);
	if (c_host_manager->whois_records == NULL) {
		free(c_host_manager->hosts);
		return 1;
	}
	c_host_manager->known_whois_records = 0;
	c_host_manager->current_whois_record_capacity = WHOIS_CAPACITY_INCREMENT_SIZE;
	memset(c_host_manager->whois_records, 0, (sizeof(struct whois_record) * WHOIS_CAPACITY_INCREMENT_SIZE));
	return 0;
}

int destroy_host_manager(host_manager *c_host_manager) {
	memset(c_host_manager->hosts, 0, (sizeof(struct single_host_info) * c_host_manager->current_capacity));
	free(c_host_manager->hosts);
	c_host_manager->known_hosts = 0;
	c_host_manager->current_capacity = 0;
	
	memset(c_host_manager->whois_records, 0, (sizeof(struct whois_record) * c_host_manager->current_whois_record_capacity));
	free(c_host_manager->whois_records);
	c_host_manager->known_whois_records = 0;
	c_host_manager->current_capacity = 0;
	return 0;
}

int host_manager_add_host(host_manager *c_host_manager, single_host_info *new_host) {
	unsigned int current_host_i;
	whois_record *who_data;
	char logStr[LOGGING_STR_LEN + 1];
	for (current_host_i = 0; current_host_i < c_host_manager->known_hosts; current_host_i++) {
		if ((strncmp(new_host->hostname, c_host_manager->hosts[current_host_i].hostname, DNS_MAX_FQDN_LENGTH) == 0) && (memcmp(&new_host->ipv4_addr, &c_host_manager->hosts[current_host_i].ipv4_addr, sizeof(struct in_addr)) == 0)) {
			snprintf(logStr, sizeof(logStr), "skipping dupplicate host: %s", new_host->hostname);
			LOGGING_QUICK_DEBUG("kraken.host_manager", logStr)
			return 0;
		}
	}
	
	if (c_host_manager->known_hosts >= c_host_manager->current_capacity) {
		void *tmpbuffer = malloc(sizeof(struct single_host_info) * (c_host_manager->current_capacity + HOST_CAPACITY_INCREMENT_SIZE));
		if (tmpbuffer == NULL) {
			return 1;
		}
		c_host_manager->current_capacity += HOST_CAPACITY_INCREMENT_SIZE;
		memset(tmpbuffer, 0, (sizeof(single_host_info) * c_host_manager->current_capacity));
		memcpy(tmpbuffer, c_host_manager->hosts, (sizeof(struct single_host_info) * c_host_manager->known_hosts));
		free(c_host_manager->hosts);
		c_host_manager->hosts = tmpbuffer;
	}
	
	if (new_host->whois_data == NULL) {
		host_manager_get_whois(c_host_manager, &new_host->ipv4_addr, &who_data);
		if (who_data != NULL) {
			new_host->whois_data = who_data;
		}
	}
	
	memcpy(&c_host_manager->hosts[c_host_manager->known_hosts], new_host, sizeof(single_host_info));
	c_host_manager->known_hosts++;
	
	return 0;
}

int host_manager_quick_add_by_name(host_manager *c_host_manager, char *hostname) {
	single_host_info new_host;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	struct sockaddr_in *sin;
	int s;
	
	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	s = getaddrinfo(hostname, NULL, &hints, &result);
	if (s != 0) {
		LOGGING_QUICK_ERROR("kraken.host_manager", "could not resolve the hostname")
		return 1;
	}
	
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if (rp->ai_family == AF_INET) {
			sin = (struct sockaddr_in *)rp->ai_addr;
			init_single_host(&new_host);
			memcpy(&new_host.ipv4_addr, &sin->sin_addr, sizeof(struct in_addr));
			strncpy(new_host.hostname, hostname, DNS_MAX_FQDN_LENGTH);
			host_manager_add_host(c_host_manager, &new_host);
			destroy_single_host(&new_host);
		}
	}
	return 0;
}

int host_manager_add_alias_to_host(host_manager *c_host_manager, char *hostname, char* alias) {
	single_host_info *current_host;
	char (*block)[DNS_MAX_FQDN_LENGTH + 1];
	unsigned int current_host_i;
	unsigned int current_name_i;
	int duplicate = 0;
	for (current_host_i = 0; current_host_i < c_host_manager->known_hosts; current_host_i++) {
		if (strncmp(hostname, c_host_manager->hosts[current_host_i].hostname, DNS_MAX_FQDN_LENGTH) == 0) {
			current_host = &c_host_manager->hosts[current_host_i];
			if (current_host->aliases == NULL) {
				/* adding the first alias */
				current_host->aliases = malloc(DNS_MAX_FQDN_LENGTH + 1);
				memset(current_host->aliases, '\0', DNS_MAX_FQDN_LENGTH + 1);
				strncpy(current_host->aliases[0], alias, DNS_MAX_FQDN_LENGTH);
				current_host->n_aliases = 1;
			} else {
				/* adding an additional alias */
				duplicate = 0;
				for (current_name_i = 0; current_name_i < current_host->n_aliases; current_name_i++) {
					if (strncasecmp(current_host->aliases[current_name_i], alias, DNS_MAX_FQDN_LENGTH) == 0) {
						duplicate = 1;
						break;
					}
				}
				if (duplicate) {
					continue;
				}
				block = malloc((DNS_MAX_FQDN_LENGTH + 1) * (current_host->n_aliases + 1));
				if (block == NULL) {
					return 1;
				}
				memset(block, '\0', (DNS_MAX_FQDN_LENGTH + 1) * (current_host->n_aliases + 1));
				memcpy(block, current_host->aliases, (DNS_MAX_FQDN_LENGTH + 1) * current_host->n_aliases);
				strncpy(block[current_host->n_aliases], alias, DNS_MAX_FQDN_LENGTH);
				free(current_host->aliases);
				current_host->aliases = block;
				current_host->n_aliases += 1;
			}
		}
	}
	return 0;
}

int host_manager_get_host_by_addr(host_manager *c_host_manager, struct in_addr *target_ip, single_host_info **desired_host) {
	unsigned int current_host_i;
	*desired_host = NULL;
	for (current_host_i = 0; current_host_i < c_host_manager->known_hosts; current_host_i++) {
		if (memcmp(target_ip, &c_host_manager->hosts[current_host_i].ipv4_addr, sizeof(struct in_addr)) == 0) {
			*desired_host = &c_host_manager->hosts[current_host_i];
			break;
		}
	}
	return 0;
}

int host_manager_add_whois(host_manager *c_host_manager, whois_record *new_record) {
	if (c_host_manager->known_whois_records >= c_host_manager->current_whois_record_capacity) {
		void *tmpbuffer = malloc(sizeof(struct whois_record) * (c_host_manager->current_whois_record_capacity + WHOIS_CAPACITY_INCREMENT_SIZE));
		if (tmpbuffer == NULL) {
			return 1;
		}
		c_host_manager->current_whois_record_capacity += WHOIS_CAPACITY_INCREMENT_SIZE;
		memset(tmpbuffer, 0, (sizeof(whois_record) * c_host_manager->current_whois_record_capacity));
		memcpy(tmpbuffer, c_host_manager->whois_records, (sizeof(struct whois_record) * c_host_manager->known_whois_records));
		free(c_host_manager->whois_records);
		c_host_manager->whois_records = tmpbuffer;
		host_manager_sync_whois_data(c_host_manager);
	}
	
	memcpy(&c_host_manager->whois_records[c_host_manager->known_whois_records], new_record, sizeof(whois_record));
	c_host_manager->known_whois_records++;
	
	return 0;
}

int host_manager_get_whois(host_manager *c_host_manager, struct in_addr *target_ip, whois_record **desired_record) {
	/* 
	 * If there is a whois record that corresponds to the target ip then desired_record will be set to point to it
	 * otherwise, desired_record is NULL
	 */
	unsigned int current_who_i;
	whois_record *cur_who_resp;
	network_info network;
	int ret_val = 0;
	
	*desired_record = NULL;
	for (current_who_i = 0; current_who_i < c_host_manager->known_whois_records; current_who_i ++) {
		cur_who_resp = &c_host_manager->whois_records[current_who_i];
		ret_val = netaddr_cidr_str_to_nwk(cur_who_resp->cidr_s, &network);
		if (ret_val == 0) {
			if (netaddr_ip_in_nwk(target_ip, &network) == 1) {
				*desired_record = cur_who_resp;
				break;
			}
		} else {
			char logStr[LOGGING_STR_LEN + 1];
			snprintf(logStr, sizeof(logStr), "could not parse cidr address: %s", (char *)&cur_who_resp->cidr_s);
			LOGGING_QUICK_ERROR("kraken.host_manager", logStr)
		}
	}
	return 0;
}

void host_manager_sync_whois_data(host_manager *c_host_manager) {
	unsigned int current_host_i;
	single_host_info *current_host;
	whois_record *cur_who_resp = NULL;
	
	for (current_host_i = 0; current_host_i < c_host_manager->known_hosts; current_host_i++) {
		current_host = &c_host_manager->hosts[current_host_i];
		host_manager_get_whois(c_host_manager, &current_host->ipv4_addr, &cur_who_resp);
		if (cur_who_resp != NULL) {
			current_host->whois_data = cur_who_resp;
		}
	}
	return;
}
