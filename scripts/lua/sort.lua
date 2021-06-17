

local tb = {20, 10, 2, 3, 4, 89, 20, 33, 2, 3}

 

-- 默认是升序排序

table.sort(tb)

for _, v in ipairs(tb) do

    print(v)

end

 

print("=======")

 

-- 修改为降序排序

table.sort(tb, function (a, b) if a > b then return true end end)

for _, v in ipairs(tb) do

    print(v)

end


