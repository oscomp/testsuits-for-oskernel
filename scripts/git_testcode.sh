./busybox echo "#### OS COMP TEST GROUP START git ####"
./usr/bin/git config --global --add safe.directory "$(pwd)"
./usr/bin/git config --global user.email "you@example.com"
./usr/bin/git config --global user.name "Your Name"
./usr/bin/git help
./usr/bin/git init
./busybox echo "hello world" > README.md
./usr/bin/git add README.md
./usr/bin/git commit -m "add README.md"
./usr/bin/git log
./busybox echo "#### OS COMP TEST GROUP END git ####"
