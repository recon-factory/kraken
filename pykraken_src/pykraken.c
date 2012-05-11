#include <Python.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "hosts.h"
#include "host_manager.h"
#include "dns_enum.h"
#include "network_addr.h"
#include "whois_lookup.h"

#define MODULE_DOC ""
#define MODULE_VERSION "0.1"

static PyObject *pykraken_whois_lookup_ip(PyObject *self, PyObject *args) {
	struct in_addr target_ip;
	char *ipstr;
	whois_response who_resp;
	PyObject *pyWhoResp = PyDict_New();
	PyObject *pyTmpStr = NULL;
	
	if (pyWhoResp == NULL) {
		PyErr_SetString(PyExc_Exception, "could not create a dictionary to store the results");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "s", &ipstr)) {
		Py_DECREF(pyWhoResp);
		return NULL;
	}
	if (inet_pton(AF_INET, ipstr, &target_ip) == 0) {
		PyErr_SetString(PyExc_Exception, "invalid IP address");
		Py_DECREF(pyWhoResp);
		return NULL;
	}
	if (whois_lookup_ip(&target_ip, &who_resp) != 0) {
		PyErr_SetString(PyExc_Exception, "whois lookup failed");
		Py_DECREF(pyWhoResp);
		return NULL;
	}
	if (who_resp.cidr_s[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.cidr_s);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "cidr", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	if (who_resp.netname[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.netname);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "netname", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	if (who_resp.description[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.description);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "description", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	if (who_resp.orgname[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.orgname);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "orgname", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	if (who_resp.regdate_s[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.regdate_s);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "regdate", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	if (who_resp.updated_s[0] != '\0') {
		pyTmpStr = PyString_FromString(who_resp.updated_s);
		if (pyTmpStr) {
			PyDict_SetItemString(pyWhoResp, "updated", pyTmpStr);
			Py_DECREF(pyTmpStr);
		}
	}
	return pyWhoResp;
}

static PyObject *pykraken_get_nameservers(PyObject *self, PyObject *args) {
	domain_ns_list nameservers;
	char ipstr[INET_ADDRSTRLEN];
	int i;
	char *pTargetDomain;
	PyObject *nsList = PyDict_New();
	PyObject *nsIpaddr;
	
	if (nsList == NULL) {
		PyErr_SetString(PyExc_Exception, "could not create a dictionary to store the results");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "s", &pTargetDomain)) {
		Py_DECREF(nsList);
		return NULL;
	}
	memset(&nameservers, '\0', sizeof(nameservers));
	
	dns_get_nameservers_for_domain(pTargetDomain, &nameservers);
	for (i = 0; (nameservers.servers[i][0] != '\0' && i < DNS_MAX_NS_HOSTS); i++) {
		inet_ntop(AF_INET, &nameservers.ipv4_addrs[i], ipstr, sizeof(ipstr));
		nsIpaddr = PyString_FromString(ipstr);
		if (nsIpaddr) {
			PyDict_SetItemString(nsList, nameservers.servers[i], nsIpaddr);
			Py_DECREF(nsIpaddr);
		}
	}
	return nsList;
}

static PyObject *pykraken_enumerate_domain(PyObject *self, PyObject *args) {
	host_manager c_host_manager;
	single_host_info current_host;
	unsigned int current_host_i = 0;
	char *pTargetDomain;
	char ipstr[INET_ADDRSTRLEN];
	PyObject *pyTmpStr = NULL;
	PyObject *pyHostList = PyDict_New();
	
	if (pyHostList == NULL) {
		PyErr_SetString(PyExc_Exception, "could not create a dictionary to store the results");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "s", &pTargetDomain)) {
		Py_DECREF(pyHostList);
		return NULL;
	}
	
	if (init_host_manager(&c_host_manager) != 0) {
		PyErr_SetString(PyExc_Exception, "could not initialize the host manager, it is likely that there is not enough memory");
		return NULL;
	}
	
	dns_enumerate_domain(pTargetDomain, &c_host_manager);
	
	for (current_host_i = 0; current_host_i < c_host_manager.known_hosts; current_host_i++) {
		current_host = c_host_manager.hosts[current_host_i];
		inet_ntop(AF_INET, &current_host.ipv4_addr, ipstr, sizeof(ipstr));
		pyTmpStr = PyString_FromString(ipstr);
		if (pyTmpStr) {
			PyDict_SetItemString(pyHostList, current_host.hostname, pyTmpStr);
			Py_DECREF(pyTmpStr);
		} else {
			destroy_host_manager(&c_host_manager);
			PyErr_SetString(PyExc_Exception, "could not convert a C string to a Python string");
			Py_DECREF(pyHostList);
			return NULL;
		}
	}
	destroy_host_manager(&c_host_manager);
	return pyHostList;
}

static PyObject *pykraken_enumerate_network(PyObject *self, PyObject *args) {
	host_manager c_host_manager;
	single_host_info current_host;
	unsigned int current_host_i = 0;
	network_info network;
	char *pTargetDomain;
	char *pTargetNetwork;
	char ipstr[INET_ADDRSTRLEN];
	PyObject *pyTmpStr = NULL;
	PyObject *pyHostList = PyDict_New();
	
	if (pyHostList == NULL) {
		PyErr_SetString(PyExc_Exception, "could not create a dictionary to store the results");
		return NULL;
	}
	
	if (!PyArg_ParseTuple(args, "ss", &pTargetDomain, &pTargetNetwork)) {
		Py_DECREF(pyHostList);
		return NULL;
	}
	
	if (netaddr_cidr_str_to_nwk(pTargetNetwork, &network) != 0) {
		PyErr_SetString(PyExc_Exception, "invalid CIDR network");
		Py_DECREF(pyHostList);
		return NULL;
	}
	
	if (init_host_manager(&c_host_manager) != 0) {
		PyErr_SetString(PyExc_Exception, "could not initialize the host manager, it is likely that there is not enough memory");
		return NULL;
	}
	
	dns_enumerate_network(pTargetDomain, &network, &c_host_manager);
	
	for (current_host_i = 0; current_host_i < c_host_manager.known_hosts; current_host_i++) {
		current_host = c_host_manager.hosts[current_host_i];
		inet_ntop(AF_INET, &current_host.ipv4_addr, ipstr, sizeof(ipstr));
		pyTmpStr = PyString_FromString(ipstr);
		if (pyTmpStr) {
			PyDict_SetItemString(pyHostList, current_host.hostname, pyTmpStr);
			Py_DECREF(pyTmpStr);
		} else {
			destroy_host_manager(&c_host_manager);
			PyErr_SetString(PyExc_Exception, "could not convert a C string to a Python string");
			Py_DECREF(pyHostList);
			return NULL;
		}
	}
	destroy_host_manager(&c_host_manager);
	return pyHostList;
}

static PyObject *pykraken_ip_in_cidr(PyObject *self, PyObject *args) {
	char *pIpAddr;
	char *pCidrNetwork;
	network_info network;
	struct in_addr packedIp;
	
	if (!PyArg_ParseTuple(args, "ss", &pIpAddr, &pCidrNetwork)) {
		return NULL;
	}
	
	if (netaddr_cidr_str_to_nwk(pCidrNetwork, &network) != 0) {
		PyErr_SetString(PyExc_Exception, "invalid CIDR network");
		return NULL;
	}
	
	if (inet_pton(AF_INET, pIpAddr, &packedIp) == 0) {
		PyErr_SetString(PyExc_Exception, "invalid IP address");
		return NULL;
	}
	
	if (netaddr_ip_in_nwk(&packedIp, &network) == 1) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

static PyMethodDef PyKrakenMethods[] = {
	{"whois_lookup_ip", pykraken_whois_lookup_ip, METH_VARARGS, "whois_lookup_ip(target_ip)\nRetrieve the whois record pretaining to an IP address\n\n@type target_ip: String\n@param target_ip: ip address to retreive whois information for"},
	{"get_nameservers", pykraken_get_nameservers, METH_VARARGS, "get_nameservers(target_domain)\nEnumerate nameservers for a domain\n\n@type target_domain: String\n@param target_domain: the domain to retreive the list of name servers for"},
	{"enumerate_domain", pykraken_enumerate_domain, METH_VARARGS, "enumerate_domain(target_domain)\nEnumerate hostnames for a domain\n\n@type target_domain: String\n@param target_domain: the domain to enumerate hostnames for"},
	{"enumerate_network", pykraken_enumerate_network, METH_VARARGS, "enumerate_network(target_domain, target_network)\nEnumerate hostnames for a network\n\n@type target_domain: String\n@param target_domain: the domain who's name servers to use\n@type target_network: String\n@param target_network: the network in CIDR notation to bruteforce records for"},
	{"ip_in_cidr", pykraken_ip_in_cidr, METH_VARARGS, "ip_in_cidr(target_ip, target_network)\nCheck if an IP address is in a CIDR network\n\n@type target_ip: String\n@param target_ip: the ip to check\n@type target_network: String\n@param target_network: the network to check"},
	{NULL, NULL, 0, NULL}
};

void initpykraken(void) {
   	PyObject *mod;
	mod = Py_InitModule3("pykraken", PyKrakenMethods, MODULE_DOC);
	PyModule_AddStringConstant(mod, "version", MODULE_VERSION);
	return;
}

int main(int argc, char *argv[]) {
	Py_SetProgramName(argv[0]);
	Py_Initialize();
	initpykraken();
	return 0;
}
