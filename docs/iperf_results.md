====== iperf BASIC_UDP begin ======
Connecting to host 127.0.0.1, port 5001
[  5] local 127.0.0.1 port 48004 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Total Datagrams
[  5]   0.00-2.00   sec   121 MBytes   506 Mbits/sec  3863  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-2.00   sec   121 MBytes   506 Mbits/sec  0.000 ms  0/3863 (0%)  sender
[  5]   0.00-2.00   sec   120 MBytes   504 Mbits/sec  0.084 ms  13/3863 (0.34%)  receiver

iperf Done.
====== iperf BASIC_UDP end: success ======

====== iperf BASIC_TCP begin ======
Connecting to host 127.0.0.1, port 5001
[  5] local 127.0.0.1 port 51190 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-2.00   sec   303 MBytes  1.27 Gbits/sec    0   1.75 MBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-2.00   sec   303 MBytes  1.27 Gbits/sec    0             sender
[  5]   0.00-2.00   sec   294 MBytes  1.23 Gbits/sec                  receiver

iperf Done.
====== iperf BASIC_TCP end: success ======

====== iperf PARALLEL_UDP begin ======
Connecting to host 127.0.0.1, port 5001
[  5] local 127.0.0.1 port 52753 connected to 127.0.0.1 port 5001
[  7] local 127.0.0.1 port 59254 connected to 127.0.0.1 port 5001
[  9] local 127.0.0.1 port 55745 connected to 127.0.0.1 port 5001
[ 11] local 127.0.0.1 port 42433 connected to 127.0.0.1 port 5001
[ 13] local 127.0.0.1 port 52247 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Total Datagrams
[  5]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  987  
[  7]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  987  
[  9]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  987  
[ 11]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  987  
[ 13]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  987  
[SUM]   0.00-2.00   sec   154 MBytes   646 Mbits/sec  4935  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  0.000 ms  0/987 (0%)  sender
[  5]   0.00-2.01   sec  29.1 MBytes   122 Mbits/sec  0.770 ms  55/987 (5.6%)  receiver
[  7]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  0.000 ms  0/987 (0%)  sender
[  7]   0.00-2.01   sec  29.1 MBytes   121 Mbits/sec  0.768 ms  57/987 (5.8%)  receiver
[  9]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  0.000 ms  0/987 (0%)  sender
[  9]   0.00-2.01   sec  28.9 MBytes   121 Mbits/sec  0.758 ms  62/987 (6.3%)  receiver
[ 11]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  0.000 ms  0/987 (0%)  sender
[ 11]   0.00-2.01   sec  28.8 MBytes   120 Mbits/sec  0.839 ms  67/987 (6.8%)  receiver
[ 13]   0.00-2.00   sec  30.8 MBytes   129 Mbits/sec  0.000 ms  0/987 (0%)  sender
[ 13]   0.00-2.01   sec  28.7 MBytes   120 Mbits/sec  0.890 ms  69/987 (7%)  receiver
[SUM]   0.00-2.00   sec   154 MBytes   646 Mbits/sec  0.000 ms  0/4935 (0%)  sender
[SUM]   0.00-2.01   sec   145 MBytes   604 Mbits/sec  0.805 ms  310/4935 (6.3%)  receiver

iperf Done.
====== iperf PARALLEL_UDP end: success ======

====== iperf PARALLEL_TCP begin ======
Connecting to host 127.0.0.1, port 5001
[  5] local 127.0.0.1 port 51196 connected to 127.0.0.1 port 5001
[  7] local 127.0.0.1 port 51198 connected to 127.0.0.1 port 5001
[  9] local 127.0.0.1 port 51200 connected to 127.0.0.1 port 5001
[ 11] local 127.0.0.1 port 51202 connected to 127.0.0.1 port 5001
[ 13] local 127.0.0.1 port 51204 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0   1.81 MBytes       
[  7]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0   1.87 MBytes       
[  9]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0   1.12 MBytes       
[ 11]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0   1.62 MBytes       
[ 13]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0   1.62 MBytes       
[SUM]   0.00-2.02   sec   375 MBytes  1.55 Gbits/sec    0             
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0             sender
[  5]   0.00-2.03   sec  75.0 MBytes   310 Mbits/sec                  receiver
[  7]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0             sender
[  7]   0.00-2.03   sec  75.0 MBytes   310 Mbits/sec                  receiver
[  9]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0             sender
[  9]   0.00-2.03   sec  74.9 MBytes   309 Mbits/sec                  receiver
[ 11]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0             sender
[ 11]   0.00-2.03   sec  74.9 MBytes   309 Mbits/sec                  receiver
[ 13]   0.00-2.02   sec  75.0 MBytes   311 Mbits/sec    0             sender
[ 13]   0.00-2.03   sec  74.9 MBytes   309 Mbits/sec                  receiver
[SUM]   0.00-2.02   sec   375 MBytes  1.55 Gbits/sec    0             sender
[SUM]   0.00-2.03   sec   375 MBytes  1.55 Gbits/sec                  receiver

iperf Done.
====== iperf PARALLEL_TCP end: success ======

====== iperf REVERSE_UDP begin ======
Connecting to host 127.0.0.1, port 5001
Reverse mode, remote host 127.0.0.1 is sending
[  5] local 127.0.0.1 port 54859 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-2.00   sec   115 MBytes   482 Mbits/sec  0.036 ms  5/3680 (0.14%)  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-2.00   sec   115 MBytes   482 Mbits/sec  0.000 ms  0/3680 (0%)  sender
[  5]   0.00-2.00   sec   115 MBytes   482 Mbits/sec  0.036 ms  5/3680 (0.14%)  receiver

iperf Done.
====== iperf REVERSE_UDP end: success ======

====== iperf REVERSE_TCP begin ======
Connecting to host 127.0.0.1, port 5001
Reverse mode, remote host 127.0.0.1 is sending
[  5] local 127.0.0.1 port 51210 connected to 127.0.0.1 port 5001
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-2.00   sec   279 MBytes  1.17 Gbits/sec                  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-2.01   sec   289 MBytes  1.20 Gbits/sec    0             sender
[  5]   0.00-2.00   sec   279 MBytes  1.17 Gbits/sec                  receiver

iperf Done.
====== iperf REVERSE_TCP end: success ======