#include "issueurl.h"
#include "urlencode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *build_issue_url(const struct issue_report *report) {
    const char *base =
        "https://github.com/ddccontrol/ddccontrol-db/issues/new"
        "?template=unsupported-monitor.yml";

    char *enc_name = url_encode(report->monitor_name ? report->monitor_name : "");
    char *enc_pnp  = url_encode(report->pnp_id ? report->pnp_id : "");
    // char *enc_dev  = url_encode(report->device ? report->device : "");
    // char *enc_ver  = url_encode(report->ddccontrol_version ? report->ddccontrol_version : "");

    printf("enc_name: %s\n", report->monitor_name);

    if (!enc_name || !enc_pnp || !enc_dev || !enc_ver) {
        free(enc_name);
        free(enc_pnp);
        // free(enc_dev);
        // free(enc_ver);
        return NULL;
    }

    size_t len = strlen(base)
               + strlen("&monitor_name=") + strlen(enc_name)
               + strlen("&pnp_id=") + strlen(enc_pnp)
               // + strlen("&device=") + strlen(enc_dev)
               // + strlen("&ddccontrol_version=") + strlen(enc_ver)
               + 1;

    char *url = malloc(len);
    if (!url) {
        free(enc_name);
        free(enc_pnp);
        // free(enc_dev);
        // free(enc_ver);
        return NULL;
    }
// &device=%s&ddccontrol_version=%s
// , enc_dev, enc_ver
    snprintf(url, len,
             "%s&monitor_name=%s&pnp_id=%s",
             base, enc_name, enc_pnp);

    free(enc_name);
    free(enc_pnp);
    // free(enc_dev);
    // free(enc_ver);

    return url;
}
