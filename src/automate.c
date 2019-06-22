#include <platform.h>

#include "kernel/faction.h"
#include "kernel/messages.h"
#include "kernel/order.h"
#include "kernel/region.h"
#include "kernel/unit.h"

#include "util/keyword.h"
#include "util/log.h"

#include "automate.h"
#include "laws.h"
#include "study.h"

#include <stdlib.h>
#include <assert.h>

static int cmp_scholars(const void *lhs, const void *rhs)
{
    const scholar *a = (const scholar *)lhs;
    const scholar *b = (const scholar *)rhs;
    if (a->sk == b->sk) {
        /* sort by level, descending: */
        return b->level - a->level;
    }
    /* order by skill */
    return (int)a->sk - (int)b->sk;
}

int autostudy_init(scholar scholars[], int max_scholars, unit **units)
{
    unit *unext = NULL, *u = *units;
    faction *f = u->faction;
    int nscholars = 0;

    while (u) {
        keyword_t kwd = init_order(u->thisorder, u->faction->locale);
        if (kwd == K_AUTOSTUDY) {
            if (long_order_allowed(u)) {
                if (f == u->faction) {
                    scholar * st = scholars + nscholars;
                    skill_t sk = getskill(u->faction->locale);
                    if (check_student(u, u->thisorder, sk)) {
                        st->sk = sk;
                        st->level = effskill_study(u, st->sk);
                        st->learn = 0;
                        st->u = u;
                        if (++nscholars > max_scholars) {
                            log_fatal("you must increase MAXSCHOLARS");
                        }
                    }
                }
                else if (!unext) {
                    unext = u;
                }
            }
        }
        u = u->next;
    }
    *units = unext;
    if (nscholars > 0) {
        qsort(scholars, nscholars, sizeof(scholar), cmp_scholars);
    }
    return nscholars;
}

static void teaching(scholar *s, int n) {
    assert(n <= s->u->number);
    s->learn += n;
    s->u->flags |= UFL_LONGACTION;
}

static void learning(scholar *s, int n) {
    assert(n <= s->u->number);
    s->learn += n;
    s->u->flags |= UFL_LONGACTION;
}

void autostudy_run(scholar scholars[], int nscholars)
{
    int ti = 0;
    while (ti != nscholars) {
        skill_t sk = scholars[ti].sk;
        int t, se, ts = 0, tt = 0, si = ti;
        for (se = ti; se != nscholars && scholars[se].sk == sk; ++se) {
            int mint;
            ts += scholars[se].u->number; /* count total scholars */
            mint = (ts + 10) / 11; /* need a minimum of ceil(ts/11) teachers */
            for (; mint > tt && si != nscholars && scholars[si].sk == sk; ++si) {
                tt += scholars[si].u->number;
            }
        }
        /* now si splits the teachers and students 1:10 */
        /* first student must be 2 levels below first teacher: */
        for (; si != se && scholars[si].sk == sk; ++si) {
            if (scholars[si].level + TEACHDIFFERENCE <= scholars[ti].level) {
                break;
            }
            tt += scholars[si].u->number;
        }
        /* now si is the first unit we can teach, if we can teach any */
        if (si == se || scholars[si].sk != sk) {
            /* there are no students, so standard learning for everyone */
            for (t = ti; t != se; ++t) {
                learning(scholars + t, scholars[t].u->number);
            }
        }
        else {
            /* invariant: unit ti can still teach i students */
            int i = scholars[ti].u->number * STUDENTS_PER_TEACHER;
            /* invariant: unit si has n students that can still be taught */
            int s, n = scholars[si].u->number;
            for (t = ti, s = si; t != si && s != se; ) {
                if (i >= n) {
                    /* t has more than enough teaching capacity for s */
                    i -= n;
                    teaching(scholars + s, n);
                    learning(scholars + s, scholars[s].u->number);
                    /* next student, please: */
                    if (++s == se) {
                        continue;
                    }
                    n = scholars[s].u->number;
                }
                else {
                    /* a part of s gets credited and we need a new teacher: */
                    teaching(scholars + s, i);
                    /* we still need to teach n students in this unit: */
                    n -= i;
                    i = 0;
                    /* we want a new teacher for s. if any exists, it's next in the sequence. */
                    if (++t == si) {
                        continue;
                    }
                    if (scholars[t].level - TEACHDIFFERENCE < scholars[s].level) {
                        /* no remaining teacher can teach this student, so we skip ahead */
                        do {
                            /* remaining students learn without a teacher: */
                            learning(scholars + s, n);
                            if (++s == se) {
                                break;
                            }
                            n = scholars[s].u->number;
                        } while (scholars[t].level - TEACHDIFFERENCE < scholars[s].level);
                    }
                    i = scholars[t].u->number * STUDENTS_PER_TEACHER;
                }
            }
            if (i > 0) {
                int remain = (STUDENTS_PER_TEACHER * scholars[t].u->number - i + STUDENTS_PER_TEACHER - 1) / STUDENTS_PER_TEACHER;
                /* teacher has remaining time */
                learning(scholars + t, remain);
            }
            ++t;
            for (; t < si; ++t) {
                learning(scholars + t, scholars[t].u->number);
            }
        }
        ti = se;
    }
}

#define MAXSCHOLARS 2048

void do_autostudy(region *r)
{
    static int max_scholars;
    unit *units = r->units;
    scholar scholars[MAXSCHOLARS];
    while (units) {
        int i, nscholars = autostudy_init(scholars, MAXSCHOLARS, &units);
        if (nscholars > max_scholars) {
            stats_count("automate.max_scholars", nscholars - max_scholars);
            max_scholars = nscholars;
        }
        autostudy_run(scholars, nscholars);
        for (i = 0; i != nscholars; ++i) {
            int days = STUDYDAYS * scholars[i].learn;
            learn_skill(scholars[i].u, scholars[i].sk, days);
        }
    }
}
