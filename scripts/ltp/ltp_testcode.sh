#!/bin/bash

echo "#### OS COMP TEST GROUP START ltp ####"

# 定义目标目录
target_dir="ltp/testcases/bin"

# 遍历目录下的所有文件
for file in "$target_dir"/*; do
  # 跳过目录，仅处理文件
  if [ -f "$file" ]; then
    # 输出文件名
    echo "RUN LTP CASE $(basename "$file")"

    "$file"
    ret=$?

    # 输出文件名和返回值
    echo "FAIL LTP CASE $(basename "$file") : $ret"
  fi
done


echo "#### OS COMP TEST GROUP END ltp ####"