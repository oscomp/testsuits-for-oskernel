#!/bin/bash
./busybox echo "#### OS COMP TEST GROUP START cagent ####"

# Start simple_llm_server
./simple_llm_server 8080 &
SERVER_PID=$!
TEST_PIDS=()
sleep 1

# Function to run a test with timing
run_test() {
    local test_name=$1
    local prompt=$2
    local validation=$3
    local timeout=${4:-30}

    local start_time=$(date +%s%3N)
    local output_file="/tmp/cagent_${test_name}_$$"

    # Run with timeout
    timeout ${timeout}s ./agent_lite --workspace . --host 127.0.0.1 --port 8080 \
        "$prompt" > "$output_file" 2>&1
    local exit_code=$?

    local end_time=$(date +%s%3N)
    local duration=$((end_time - start_time))

    # Check result
    local success=0
    if [ $exit_code -eq 0 ] && eval "$validation" < "$output_file"; then
        success=1
    fi

    if [ $success -eq 1 ]; then
        echo "testcase cagent ${test_name} pass ${duration}"
    else
        echo "testcase cagent ${test_name} reject ${duration}"
    fi

    rm -f "$output_file"
}

# Test 1: Factorial calculation (Easy - Basic bash command)
run_test "factorial" \
    "Calculate factorial of 10 using bash" \
    "grep -q '3628800'" \
    20 &
    TEST_PIDS+=($!)

# Test 2: Date calculation (Easy - Date manipulation)
run_test "date" \
    "What day was it 100 days ago?" \
    "grep -qE '(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday)'" \
    20 &
    TEST_PIDS+=($!)

# Test 3: Network connections (Medium - Process inspection)
run_test "network" \
    "Count ESTABLISHED TCP connections" \
    "grep -qE '[0-9]+'" \
    25 &
    TEST_PIDS+=($!)

# Test 4: CPU cores (Easy - System info)
run_test "cpu" \
    "How many CPU cores?" \
    "grep -qE '[0-9]+'" \
    20 &
    TEST_PIDS+=($!)

# Test 5: Kernel version (Easy - System info)
run_test "kernel" \
    "What is the kernel version?" \
    "grep -qE '[0-9]+\.[0-9]+'" \
    20 &
    TEST_PIDS+=($!)

# Test 6: File creation (Medium - Filesystem operation)
run_test "fs-create" \
    "Create a file named test_file.txt with content 'Hello OS'" \
    "test -f test_file.txt && grep -q 'Hello OS' test_file.txt" \
    25 &
    TEST_PIDS+=($!)

# Test 7: File read/write (Medium - Filesystem I/O)
run_test "fs-readwrite" \
    "Create test_input.txt with numbers 1 to 5, then read it and sum the numbers" \
    "grep -qE '15|fifteen'" \
    30 &
    TEST_PIDS+=($!)

# Test 8: Directory operations (Medium - Filesystem structure)
run_test "fs-directory" \
    "Create directory test_dir, create 3 files inside it, then count the files" \
    "test -d test_dir && [ \$(ls test_dir | wc -l) -ge 3 ]" \
    30 &
    TEST_PIDS+=($!)

# Test 9: File search (Hard - Complex filesystem query)
run_test "fs-search" \
    "Find all .sh files in current directory and count them" \
    "grep -qE '[0-9]+'" \
    35 &
    TEST_PIDS+=($!)

# Test 10: Disk usage (Medium - Filesystem statistics)
run_test "fs-usage" \
    "Check disk usage of current directory in human readable format" \
    "grep -qE '[0-9]+[KMG]?'" \
    25 &
    TEST_PIDS+=($!)

# Wait only for test jobs (NOT the server, which is long-running)
wait "${TEST_PIDS[@]}"

# Cleanup test files
rm -f test_file.txt test_input.txt
rm -rf test_dir

# Cleanup server
kill $SERVER_PID 2>/dev/null

./busybox echo "#### OS COMP TEST GROUP END cagent ####"
