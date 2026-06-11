/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#ifndef ISSUEURL_H
#define ISSUEURL_H

struct issue_report {
    const char *monitor_name;
    const char *pnp_id;
    const char *device;
    const char *ddccontrol_version;
    const char *fallback_profile;
};

char *build_issue_url(const struct issue_report *report);

#endif
