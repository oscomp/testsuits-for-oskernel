-- Opens a file in append mode
file = io.open("test.txt", "w")

-- appends a word test to the last line of the file
file:write("--test\n")

-- closes the open file
file:close()
