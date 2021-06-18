

local tbCurrentTime = os.date("*t")

 

for k, v in pairs(tbCurrentTime) do

    print(k .. "=" .. tostring(v))

end


