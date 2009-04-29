require "bitstring"
require "os"

-- well aligned binary
local test1_input = string.rep("A", 1024 * 16)
local test1 = function()
    result = bitstring.pack("16:int, all:bin, 16:int", 
        1, 
        test1_input,
        2)
end

local test2 = function()
   result = bitstring.pack(
   [[
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int,
   32:int
   ]], 
   1, 2, 3, 4,
   1, 2, 3, 4,
   1, 2, 3, 4,
   1, 2, 3, 4)
end

local profile = function()
    os.exit(0)
    local loops = 1000000
    local t1 = os.time()
    for i = 1, loops do
        test1()
        test2()
        print("running loop: ", i)
    end
    local t2 = os.time()
    print("running time for ", loops, " loops is ", os.difftime(t2, t1), " seconds")
end

profile()
