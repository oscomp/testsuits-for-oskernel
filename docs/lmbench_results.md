#### 在qemu上的运行结果

```
064005
latency measurements
Simple syscall: 0.6571 microseconds
Simple read: 16.1866 microseconds
Simple write: 1.2170 microseconds
mkdir: cannot create directory ‘/var/tmp’: File exists
Simple stat: 108.0042 microseconds
Simple fstat: 17.4874 microseconds
Simple open/close: 140.5263 microseconds
Select on 100 fd's: 35.5793 microseconds
Signal handler installation: 17.1532 microseconds
Signal handler overhead: 122.5584 microseconds
Pipe latency: 186.2234 microseconds
Process fork+exit: 3290.5000 microseconds
Process fork+execve: 3642.8667 microseconds
Process fork+/bin/sh -c: 69558.0000 microseconds
File /var/tmp/XXX write bandwidth:30346 KB/sec
Pagefaults on /var/tmp/XXX: 10.1877 microseconds
0.524288 283
file system latency
0k      23      4081    5272
1k      12      2332    4067
4k      12      2317    4082
10k     9       1542    3876
Bandwidth measurements
Pipe bandwidth: 94.72 MB/sec
0.524288 315.08
0.524288 285.87
0.524288 6862.16
0.524288 311.64
context switch overhead

"size=32k ovr=60.47
2 41.47
4 43.22
8 42.48
16 44.38
24 45.48
32 49.52
64 50.67
96 51.64
064336
```

#### 在D1开发板上的运行结果

```
073430
latency measurements
Simple syscall: 0.3969 microseconds
Simple read: 0.9958 microseconds
Simple write: 0.6855 microseconds
mkdir: cannot create directory ‘/var/tmp’: File exists
Simple stat: 6.3198 microseconds
Simple fstat: 0.9349 microseconds
Simple open/close: 19.3684 microseconds
Select on 100 fd's: 10.2226 microseconds
Signal handler installation: 1.0861 microseconds
Signal handler overhead: 7.5202 microseconds
Pipe latency: 40.6122 microseconds
Process fork+exit: 1770.7500 microseconds
Process fork+execve: 2004.3333 microseconds
Process fork+/bin/sh -c: 12740.0000 microseconds
File /var/tmp/XXX write bandwidth:5617 KB/sec
Pagefaults on /var/tmp/XXX: 3.8195 microseconds
0.524288 161
file system latency
0k      30      5331    5777
1k      19      3514    4606                                             
4k      18      3474    4557
10k     14      2551    4236
Bandwidth measurements
Pipe bandwidth: 266.12 MB/sec
0.524288 495.39
0.524288 458.86
0.524288 2069.56
0.524288 558.64
context switch overhead

"size=32k ovr=32.98
2 23.23
4 29.14
8 29.14
16 28.91
24 29.45
32 29.09
64 28.84
96 29.18
073732
```

