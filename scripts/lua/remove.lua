file = io.open("test.txt", "r")

if file then
    file:close()
    os.remove("test.txt")
end
