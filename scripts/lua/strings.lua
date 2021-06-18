

local str = "Jelly Think"

 

-- string.len可以获得字符串的长度

local len = string.len(str)

print(len) -- 11

 

-- string.rep返回字符串重复n次的结果

str = "ab"

local newStr = string.rep(str, 2) -- 重复两次

print(newStr) -- abab

 

-- string.lower将字符串小写变成大写形式，并返回一个改变以后的副本

str = "Jelly Think"

newStr = string.lower(str)

print(newStr) -- jelly think

 

-- string.upper将字符串大写变成小写形式，并返回一个改变以后的副本

newStr = string.upper(str)

print(newStr) -- JELLY THINK


