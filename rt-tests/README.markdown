# RT-Tests

This repository contains some programs that test various rt-linux features.

## Usage

### Compile

    sudo apt-get install build-essential libnuma-dev
    make

### Run tests

To run one test thread per CPU or per CPU core, each thread on a separate
processor, type

    sudo ./cyclictest -a -t -n -p99

On a non-realtime system, you may see something like

    T: 0 ( 3431) P:99 I:1000 C: 100000 Min:      5 Act:   10 Avg:   14 Max:   39242
    T: 1 ( 3432) P:98 I:1500 C:  66934 Min:      4 Act:   10 Avg:   17 Max:   39661

The rightmost column contains the most important result, i.e. the worst-case
latency of 39.242 milliseconds. On a realtime-enabled system, the result may
look like

    T: 0 ( 3407) P:99 I:1000 C: 100000 Min:      7 Act:   10 Avg:   10 Max:      18
    T: 1 ( 3408) P:98 I:1500 C:  67043 Min:      7 Act:    8 Avg:   10 Max:      22

and, thus, indicate an apparent short-term worst-case latency of 18
microseconds.

Running cyclictest only over a short period of time and without creating
appropriate real-time stress conditions is rather meaningless, since the
execution of an asynchronous event from idle state is normally always quite
fast, and every - even non-RT system - can do that. The challenge is to minimize
 the latency when reacting to an asynchronuous event, irrespective of what code
path is executed at the time when the external event arrives.
Therefore, specific stress conditions must be present while cyclictest is
running to reliably determine the worst-case latency of a given system.

### Latency fighting

If - as in the above example - a low worst-case latency is measured, and this is
the case even under a system load that is equivalent to the load expected under
production conditions, everything is alright.
Of course, the measurement must last suffciently long, preferably 24 hours or
more to run several hundred million test threads. If possible, the `-i` command
line option (thread interval) should be used to increase the number of test
threads over time.
As a role of thumb, the thread interval should be set to a value twice as long
as the expected worst-case latency. If at the end of such a test period the
worst-cae latency still did not exceed the value that is assumed critical for a
given system, the particular kernel in combination with the hardware in use can
then probably be regarded as real-time capable.

What, however, if the latency is higher than acceptable? Then, the famous
"*latency fighting*" begins. For this purpose, the cyclictest tool provides the
`-b` option that causes a function tracing to be written to
`/sys/kernel/debug/tracing/trace`, if a specified latency threshold was
exceeded, for example:

    ./cyclictest -a -t -n -p99 -f -b100

This causes the program to abort execution, if the latency value exceeds 100
microseconds; the culprit can then be found in the trace output at
`/sys/kernel/debug/tracing/trace`.
The kernel function that was executed just before a latency of more than 100
microseconds was detected is marked with an exclamation mark such as

    qemu-30047 2D.h3 742805us : __activate_task+0x42/0x68 <cyclicte-426> (199 1)
    qemu-30047 2D.h3 742806us : __trace_start_sched_wakeup+0x40/0x161 <cyclicte-426> (0 -1)
    qemu-30047 2DNh3 742806us!: try_to_wake_up+0x422/0x460 <cyclicte-426> (199 -5)
    qemu-30047 2DN.1 742939us : __sched_text_start+0xf3/0xdcd (c064e442 0)

The first column indicates the calling process responsible for triggering the
latency.

If the trace output is not obvious, it can be submitted to the OSADL Latency
Fight Support Service at
[latency-fighters@osadl.org](mailto:latency-fighters@osadl.org).
In addition to the output of `cat /sys/kernel/debug/tracing/trace`, the output
of `lspci` and the `.config` file that was used to build the kernel in question
must be submitted. We are sure you understand that OSADL members will be served
first, but we promise to do our best to help everybody to successfully fight
against kernel and driver latencies.
