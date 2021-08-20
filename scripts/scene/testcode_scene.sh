
busybox mkdir -p /var/tmp
lmbench_all lmdd label="File /var/tmp/XXX write bandwidth:" of=/var/tmp/XXX move=1m fsync=1 print=3

busybox echo START bw_file_rd_io_only
lmbench_all bw_file_rd -P 1 512k io_only /var/tmp/XXX
busybox echo END bw_file_rd io_only $?

busybox echo START bw_file_rd_open2close
lmbench_all bw_mmap_rd -P 1 512k open2close /var/tmp/XXX
busybox echo END bw_file_rd open2close $?

busybox echo START lat_ctx
lmbench_all lat_ctx -P 1 -s 32 2 4 8 16 24 32
busybox echo END lat_ctx $?

busybox echo START lat_proc_fork
lmbench_all lat_proc -P 1 fork
busybox echo END lat_proc_fork $?

busybox echo START lat_proc_exec
lmbench_all lat_proc -P 1 exec
busybox echo END lat_proc_exec $?

busybox echo START bw_pipe
lmbench_all bw_pipe -P 1
busybox echo END bw_pipe $?

busybox echo START lat_pipe
lmbench_all lat_pipe -P 1
busybox echo END lat_pipe $?

busybox echo START lat_pagefault
lmbench_all lat_pagefault -P 1 /var/tmp/XXX
busybox echo END lat_pagefault $?

busybox echo START lat_mmap
lmbench_all lat_mmap -P 1 512k /var/tmp/XXX
busybox echo END lat_mmap $?

