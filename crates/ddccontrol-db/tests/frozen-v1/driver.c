/* Frozen first-reader probe. Never communicates with display hardware. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>

struct value { unsigned char *id, *name; unsigned char value; struct value *next; };
struct wide_value { struct value public_value; uint16_t value16; };
struct control {
    unsigned char *id, *name;
    unsigned char address;
    int delay, type, refresh;
    struct control *next;
    struct value *values;
};
struct subgroup { unsigned char *name, *pattern; struct subgroup *next; struct control *controls; };
struct group { unsigned char *name; struct group *next; struct subgroup *subgroups; };
struct monitor { unsigned char *name; int init; struct group *groups; };
struct vcp { int count; unsigned short *values; };
struct caps { struct vcp *vcp[256]; int type; char *raw; };
extern int ddcci_init_db(char *);
extern void ddcci_release_db(void);
extern struct monitor *ddcci_create_db(const char *, struct caps *, int);
extern void ddcci_free_db(struct monitor *);
extern int ddcci_db_requirements_failed(void);

int main(int argc, char **argv)
{
    assert(argc == 3);
    setlocale(LC_ALL, "");
    const char *localedir = getenv("DDCCONTROL_TEST_LOCALEDIR");
    if (localedir) bindtextdomain("ddccontrol-db", localedir);
    struct caps caps = {0};
    for (size_t i = 0; i < 256; i++) {
        caps.vcp[i] = calloc(1, sizeof(struct vcp));
        assert(caps.vcp[i]);
        caps.vcp[i]->count = -1;
    }
    assert(ddcci_init_db(argv[1]) == 1);
    struct monitor *monitor = ddcci_create_db(argv[2], &caps, 1);
    int failure = ddcci_db_requirements_failed();
    ddcci_release_db();
    if (!monitor) return failure ? 3 : 2;
    printf("monitor %s init=%d\n", monitor->name, monitor->init);
    for (struct group *g = monitor->groups; g; g = g->next) {
        printf("group %s\n", g->name);
        for (struct subgroup *s = g->subgroups; s; s = s->next) {
            printf("subgroup %s pattern=%s\n", s->name, s->pattern ? (char *)s->pattern : "(absent)");
            for (struct control *c = s->controls; c; c = c->next) {
                printf("control %s %s address=%u delay=%d type=%d refresh=%d\n",
                    c->id, c->name, c->address, c->delay, c->type, c->refresh);
                for (struct value *v = c->values; v; v = v->next) {
                    printf("value %s %s value16=%u\n", v->id, v->name, ((struct wide_value *)v)->value16);
                }
            }
        }
    }
    ddcci_free_db(monitor);
    for (size_t i = 0; i < 256; i++) {
        if (caps.vcp[i]) { free(caps.vcp[i]->values); free(caps.vcp[i]); }
    }
    return 0;
}
