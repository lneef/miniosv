// The miniosv application for the kexec harness (scripts/linux-kexec): report
// that the kernel came up on every CPU after being kexec'ed from Linux, in
// the form run.py --test looks for, then power off (PSCI SYSTEM_OFF) so QEMU
// exits.

#include <cstdio>
#include <osv/power.hh>
#include <osv/sched.hh>

extern "C" void osv_app_main()
{
    printf("miniosv up after kexec: %zu cpus\n", sched::cpus.size());
    printf("KEXEC-TEST: PASS\n");
    osv::poweroff();
}
