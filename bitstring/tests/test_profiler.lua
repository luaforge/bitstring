require "bitstring"
require "os"

print = function(...) end

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

count_elements = 64
format = string.rep("8:bin,", count_elements)
bitmatch = bitstring.compile(format)
input = string.rep("01234567", count_elements)

local test3 = function()
    local unpacked_values = {bitstring.unpack(format, input)}
    local unpacked_values = {bitstring.unpack(bitmatch, input)}
end

local profile = function()
    local loops = 100000
    local t1 = os.time()
    for i = 1, loops do
        --test1()
        --test2()
        --test3()
        --print("running loop: ", i)
    end
    local t2 = os.time()
    --print("running time for ", loops, " loops is ", os.difftime(t2, t1), " seconds")
end

profile()
