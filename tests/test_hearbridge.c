#define main existing_host_tests_main
#include "test_host.c"
#undef main
#ifndef EXPECT_CFG
#define EXPECT_CFG 0
#endif
#ifndef EXPECT_BACKOFF
#define EXPECT_BACKOFF 0
#endif
static long ready_at;
static void ready(void *ud, int slot, const pad_info *info)
{
    ready_at = sim_now();
    on_connect(ud, slot, info);
}
int main(void)
{
    int kind, scenario;
    for (kind = 0; kind < 2; kind++) for (scenario = 0; scenario < 5; scenario++) {
        sim_pad p = {0};
        seen s = {0};
        sim_t *sim;
        host_t *h;
        long start;
        host_events ev = { ready, on_state, on_disconnect, &s, NULL };
        unsigned char addr[6] = {1, 2, 3, 4, 5, 6};
        unlink(DB);
        p.mode = SIM_WAIT_PAIRING;
        memcpy(p.addr, addr, 6); p.cod = 0x002508;
        p.vid = kind ? 0x057e : 0x054c; p.pid = kind ? 0x2009 : 0x05c4;
        p.needs_output = 1;
        if (!kind) p.report_len = ds4_report(p.report);
        else {
            p.basic[0] = 0x3f; p.basic[3] = 8; p.basic_len = 12;
            p.report[0] = 0x30; p.report[3] = 8;
            p.report[7] = p.report[10] = 8;
            p.report[8] = p.report[11] = 0x80; p.report_len = 49;
        }
        p.early_cfg = scenario == 1 || scenario == 2;
        p.no_cfg_repeat = scenario == 2;
        p.refuse_count = scenario == 3 ? 2 : scenario == 4 ? -1 : 0;
        sim = sim_new(&p); h = host_open(sim_hci(sim), DB, &ev);
        CHECK(h != NULL); if (!h) return 1;
        start = sim_now(); ready_at = 0;
        host_pair(h, 5);
        run(h, 3000);
        if (scenario == 4 && EXPECT_BACKOFF) CHECK(sim_channel_requests(sim) <= 4);
        run(h, scenario == 4 ? 22000 : 7000);
        if (scenario == 2) CHECK(s.connects == (EXPECT_CFG ? 1 : 0));
        else if (scenario != 4) CHECK(s.connects == 1);
        else if (EXPECT_BACKOFF) {
            /* refused for ever: one fresh link
             * after 3 refusals, then eight refusals and the link is closed */
            CHECK(sim_channel_requests(sim) == 3 + 8);         /* one fresh link (FRESH_LINK_TRIES) */
            CHECK(!sim_link_up(sim));
        }
        if (s.connects) CHECK(s.states > 0);
        printf("METRIC kind=%d case=%d ready=%ld requests=%d cfg=%d hash=%08x connected=%d\n",
               kind, scenario, ready_at ? ready_at-start : -1L,
               sim_channel_requests(sim), sim_cfg_responses(sim), sim_tx_hash(sim), s.connects);
        host_close(h); sim_free(sim);
    }
    return g_fail != 0;
}
