#ifndef ISSUEURL_H
#define ISSUEURL_H

struct issue_report {
    const char *monitor_name;
    const char *pnp_id;
    const char *device;
    const char *ddccontrol_version;
};

char *build_issue_url(const struct issue_report *report);

#endif
