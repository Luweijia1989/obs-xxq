#ifndef DNSSD_H
#define DNSSD_H

#ifdef __cplusplus
extern "C" {
#endif

#define DNSSD_ERROR_NOERROR       0
#define DNSSD_ERROR_HWADDRLEN     1
#define DNSSD_ERROR_OUTOFMEM      2
#define DNSSD_ERROR_LIBNOTFOUND   3
#define DNSSD_ERROR_PROCNOTFOUND  4

typedef struct dnssd_s dnssd_t;

dnssd_t *dnssd_init(const char *name, int name_len, const char *hw_addr, int hw_addr_len, int *error);

int dnssd_register_raop(dnssd_t *dnssd, unsigned short port);
int dnssd_register_airplay(dnssd_t *dnssd, unsigned short port);

void dnssd_unregister_raop(dnssd_t *dnssd);
void dnssd_unregister_airplay(dnssd_t *dnssd);

const char *dnssd_get_airplay_txt(dnssd_t *dnssd, int *length);
const char *dnssd_get_name(dnssd_t *dnssd, int *length);
const char *dnssd_get_hw_addr(dnssd_t *dnssd, int *length);

void dnssd_destroy(dnssd_t *dnssd);

#ifdef __cplusplus
}
#endif
#endif
