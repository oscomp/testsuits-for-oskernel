./busybox echo "#### OS COMP TEST GROUP START gitnetwork ####"
./usr/bin/git daemon --reuseaddr --enable=receive-pack --base-path=./ ./&
./usr/bin/git clone git://127.0.0.1/myproject proj
./usr/bin/git clone -j 4 git://127.0.0.1/myproject1 proj1
./busybox echo "#### OS COMP TEST GROUP END gitnetwork ####"
