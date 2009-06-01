require "os"
require "test_helpers"
require "bitstring"

print = function(...) end

local test1 = function()
    local expected = 
[[00000000: 00 01 02 03 04 05 06 07   08 09 0a 0b 0c 0d 0e 0f     ................
00000010: 10 11 12 13 14 15 16 17   18 19 1a 1b 1c 1d 1e 1f     ................
00000020: 20 21 22 23 24 25 26 27   28 29 2a 2b 2c 2d 2e 2f      !"#$%&'()*+,-./
00000030: 30 31 32 33 34 35 36 37   38 39 3a 3b 3c 3d 3e 3f     0123456789:;<=>?
00000040: 40 41 42 43 44 45 46 47   48 49 4a 4b 4c 4d 4e 4f     @ABCDEFGHIJKLMNO
00000050: 50 51 52 53 54 55 56 57   58 59 5a 5b 5c 5d 5e 5f     PQRSTUVWXYZ[\]^_
00000060: 60 61 62 63 64 65 66 67   68 69 6a 6b 6c 6d 6e 6f     `abcdefghijklmno
00000070: 70 71 72 73 74 75 76 77   78 79 7a 7b 7c 7d 7e 7f     pqrstuvwxyz{|}~.
00000080: 80 81 82 83 84 85 86 87   88 89 8a 8b 8c 8d 8e 8f     ................
00000090: 90 91 92 93 94 95 96 97   98 99 9a 9b 9c 9d 9e 9f     ................
000000a0: a0 a1 a2 a3 a4 a5 a6 a7   a8 a9 aa ab ac ad ae af     ................
000000b0: b0 b1 b2 b3 b4 b5 b6 b7   b8 b9 ba bb bc bd be bf     ................
000000c0: c0 c1 c2 c3 c4 c5 c6 c7   c8 c9 ca cb cc cd ce cf     ................
000000d0: d0 d1 d2 d3 d4 d5 d6 d7   d8 d9 da db dc dd de df     ................
000000e0: e0 e1 e2 e3 e4 e5 e6 e7   e8 e9 ea eb ec ed ee ef     ................
000000f0: f0 f1 f2 f3 f4 f5 f6 f7   f8 f9 fa fb fc fd fe ff     ................
]]
    local input = {}
    for i = 0, 255 do
        input[i + 1]  = string.char(i)
    end
    input = table.concat(input)
    assert(bitstring.hexdump(input) == expected)
end

local test2 = function()
    local expected = "00000000: 00 01 02 03                                           ....\n"
    local input = {}
    for i = 0, 3 do
        input[i + 1]  = string.char(i)
    end
    input = table.concat(input)
    assert(bitstring.hexdump(input) == expected)
end

local test3 = function()
    local expected = 
[[00000000: 00 01 02 03 04 05 06 07   08 09 0a 0b 0c 0d 0e 0f     ................
00000010: 10 11 12 13                                           ....
]]
    local input = {}
    for i = 0, 19 do
        input[i + 1]  = string.char(i)
    end
    input = table.concat(input)
    assert(bitstring.hexdump(input) == expected)
end

local test4 = function()
    local expected = "00000000: 00 01 02 03 04 05 06 07   08 09 0a 0b 0c 0d           ..............\n"
    local input = {}
    for i = 0, 13 do
        input[i + 1]  = string.char(i)
    end
    input = table.concat(input)
    assert(bitstring.hexdump(input) == expected)
end

local test5 = function()
    local expected = 
[[00000000: 00 01 02 03 04 05 06 07   08 09 0a 0b 0c 0d 0e 0f     ................
00000010: 10 11 12 13 14 15 16 17   18 19 1a                    ...........
]]
    local input = {}
    for i = 0, 26 do
        input[i + 1]  = string.char(i)
    end
    input = table.concat(input)
    assert(bitstring.hexdump(input) == expected)
end

local test6 = function()
    local input = {}
    for i = 0, 32 * 1024 do
        input[i + 1]  = string.char(i % 255)
    end
    input = table.concat(input)
    local result = bitstring.hexdump(input)
    local result = bitstring.hexstream(input)
    assert(input == bitstring.fromhexstream(result))
    assert(#result == #input * 2)
end

local test7 = function()
    local expected = "000102030405060708090a0b0c0d0e0f101112131415161718191a"
    local input = {}
    for i = 0, 26 do
        input[i + 1]  = string.char(i % 255)
    end
    input = table.concat(input)
    local result = bitstring.hexstream(input)
    assert(result == expected)
    assert(bitstring.fromhexstream(result) == input)
    assert(bitstring.fromhexstream(result, 1, -1) == input)
    assert(bitstring.fromhexstream(result, 3, -3) == string.sub(input, 2, -2))
    assert(bitstring.hexstream(input, 3, 3) == "02")
    assert(bitstring.hexstream(input, 3, 4) == "0203")
    assert(bitstring.hexstream(input, -3, -2) == "1819")
end

local test8 = function()
    test_helpers.assert_throw(function() bitstring.hexdump() end, 
        "string expected")

    test_helpers.assert_throw(function() bitstring.hexstream() end,
        "string expected")

    test_helpers.assert_throw(function() bitstring.fromhexstream() end,
        "string expected")

    test_helpers.assert_throw(
        function() 
            bitstring.fromhexstream("012") 
        end,
        "wrong format")

    test_helpers.assert_throw(
        function() 
            bitstring.fromhexstream("ABKL") 
        end,
        "wrong format")
end

local test9 = function()
    -- don't allow empty strings
    -- local expected = ""
    -- local result = bitstring.hexstream("")
    -- assert(result == expected)
    -- assert(bitstring.fromhexstream(result) == "")
    -- assert(bitstring.hexdump("") == "")
end

-- test long hex


local run_tests = function()
    test_helpers.run_test("test9", test9)
    test_helpers.run_test("test8", test8)
    test_helpers.run_test("test7", test7)
    test_helpers.run_test("test6", test6)
    test_helpers.run_test("test5", test5)
    test_helpers.run_test("test4", test4)
    test_helpers.run_test("test3", test3)
    test_helpers.run_test("test2", test2)
    test_helpers.run_test("test1", test1)
    os.exit(0)
end

 
run_tests()

