require "os"
require "bitstring"
require "test_helpers"

local test2 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little, 5:bin"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test3 = function()
    local expected = "\1"
    local packed_values = {0x01}
    local format = "8:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test4 = function()
    local expected = "\1\1\2\1\2\3\4hello"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local format = "8:int, 16:int:big, 32:int:big, 5:bin"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test5 = function()
    local expected = "\255\255"
    --result = bitstring.pack("1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
    local packed_values = {}
    local format = string.rep("1:int,", 16)
    for i = 1, 16, 1 do packed_values[i] = 1 end
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end


local test6 = function()
    local expected = "\240\16\16\32\16\32\48\79"
    local packed_values = {0x0f, 0x1, 0x0102, 0x01020304, 0x0f}
    local format = "4:int, 8:int, 16:int:big, 32:int:big, 4:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test7 = function()
    local expected = "\255\255"
    local packed_values = {0x01, 0xff, 0x7f}
    local format = "1:int, 8:int, 7:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test8 = function()
    local expected = "\255\255"
    local packed_values = {0x07, 0x3f, 0x7f}
    local format = "3:int, 6:int, 7:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test9 = function()
    LUAL_BUFFERSIZE = 8192
    local expected = string.rep("\255", LUAL_BUFFERSIZE + 1)
    local packed_values = {0x07, string.rep("\255", LUAL_BUFFERSIZE), 0x1f}
    local format = "3:int, "..LUAL_BUFFERSIZE..":bin, 5:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test10 = function()
    local expected = "\160"
    local packed_values = {0x01, 0x0, 0x01, 0x0}
    local format = "1:int, 1:int, 1:int, 5:int"
    local bitmatch = bitstring.compile(format)
    test_helpers.run_pack_unpack_test(bitmatch, bitmatch, packed_values, expected)
end

local test11 = function()
    local expected = "\170\170"
    local packed_values = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}
    local pack_bitmatch = bitstring.compile(string.rep("1:int, ", 16))
    test_helpers.run_pack_unpack_test(pack_bitmatch, pack_bitmatch, packed_values, expected)
end

local test12 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local pack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little, all:bin")
    local unpack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little, rest:bin")
    test_helpers.run_pack_unpack_test(pack_bitmatch, unpack_bitmatch, packed_values, expected)
end

local test13 = function()
    local expected = "\1\2\1\4\3\2\1hell"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local pack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little, 4:bin")
    local result = bitstring.pack(pack_bitmatch, unpack(packed_values))
    test_helpers.assert_equal(result, expected)
    local unpacked_values = {bitstring.unpack(pack_bitmatch, result)}
    packed_values[4] = "hell"
    test_helpers.assert_tables_equal(packed_values, unpacked_values)
end

local test14 = function()
    test_helpers.assert_throw(
        function() 
            bitstring.pack(bitstring.compile("8:int:little, 16:int:little, 32:int:little, 6:bin"), 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    test_helpers.assert_throw(
        function()
            bitstring.unpack(bitstring.compile("8:bin"), "hello")
        end,
        "size error")
end

local test15 = function()
    test_helpers.assert_throw(
        function() 
            bitstring.pack(
                bitstring.compile("8:int:little, 16:int:little, 33:int:little, all:bin"), 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    test_helpers.assert_throw(
        function() 
            bitstring.unpack(bitstring.compile("33:int:little"), "\1\2\3\4") 
        end,
        "size error")

    test_helpers.assert_throw(
        function() 
            bitstring.unpack(bitstring.compile("33:int:little"), "\1\2\3\4\5") 
        end,
        "size error")
    
    test_helpers.assert_throw(
        function() 
            bitstring.unpack(bitstring.compile("17:int:little"), "\1\2\3\4") 
        end,
        "wrong format")
end

local test16 = function()
    test_helpers.assert_throw(
        function() 
            bitstring.pack(
                bitstring.compile("8:int:little, 0:int:little, 32:int:little, all:bin"), 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    test_helpers.assert_throw(
        function() 
            bitstring.pack(bitstring.compile("0:int:little"), "hello")
        end,
        "size error")
end

local test17 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local pack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little, all:bin ")
    local unpack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little, rest:bin ")
    test_helpers.run_pack_unpack_test(pack_bitmatch, unpack_bitmatch, packed_values, expected)
end

local test18 = function()
    local expected = "\1\2\1\4\3\2\1"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local pack_bitmatch = bitstring.compile("8:int:little, 16:int:little, 32:int:little,     ");
    test_helpers.run_pack_unpack_test(pack_bitmatch, pack_bitmatch, packed_values, expected)
end

local test19 = function()
    test_helpers.assert_throw(
        function() 
            bitstring.pack(bitstring.compile(":int:little, 16:int:little"), 0x1, 0x0102)
        end,
        "wrong format")

    test_helpers.assert_throw(
        function() 
            bitstring.pack(bitstring.compile("8:int:little, 16:int:littl"), 0x1, 0x0102)
        end,
        "wrong format")

    test_helpers.assert_throw(
        function() 
            bitstring.pack(bitstring.compile("8:in:little, 16:int:little"), 0x1, 0x0102)
        end,
        "wrong format")
end

local test20 = function()
    int8, int16, int32 = bitstring.unpack(
        bitstring.compile("8:int, 16:int:big, 32:int:little"), "\1\1\2\1\2\3\4")
end

local test21 = function()
    local expected = "\255\255"
    local packed_values = {0xff, 0xff, 0xff}
    local format = "1:int, 8:int, 7:int"
    local result = bitstring.pack(bitstring.compile(format), unpack(packed_values))
    test_helpers.assert_equal(result, expected)
    local unpacked_values = {bitstring.unpack(bitstring.compile(format), expected)}
    local expected_values = {0x01, 0xff, 0x7f}
    test_helpers.assert_tables_equal(unpacked_values, expected_values)
    local unpacked_values = {bitstring.unpack(bitstring.compile(format), expected, 1, -1)}
    test_helpers.assert_tables_equal(unpacked_values, expected_values)
end

local test24 = function()
    local length = 1024 * 7 

    local values = {}
    local format = {}
    for i = 1, length do
        table.insert(values, i)
        table.insert(format, "32:int,")
    end
    for i = 1, 3 do
        local bitmatch = bitstring.compile(table.concat(format))
        local result = bitstring.pack(bitmatch, unpack(values))
        local unpacked_values = {bitstring.unpack(bitmatch, result, 1, -1)}
        test_helpers.assert_tables_equal(unpacked_values, values)
    end
end


local run_tests = function()
    test_helpers.disable_print()
    test_helpers.run_test("test24", test24)
    test_helpers.run_test("test21", test21)
    test_helpers.run_test("test20", test20)
    test_helpers.run_test("test19", test19)
    test_helpers.run_test("test18", test18)
    test_helpers.run_test("test17", test17)
    test_helpers.run_test("test16", test16)
    test_helpers.run_test("test15", test15)
    test_helpers.run_test("test14", test14)
    test_helpers.run_test("test13", test13)
    test_helpers.run_test("test12", test12)
    test_helpers.run_test("test11", test11)
    test_helpers.run_test("test10", test10)
    test_helpers.run_test("test9", test9)
    test_helpers.run_test("test8", test8)
    test_helpers.run_test("test7", test7)
    test_helpers.run_test("test6", test6)
    test_helpers.run_test("test5", test5)
    test_helpers.run_test("test4", test4)
    test_helpers.run_test("test3", test3)
    test_helpers.run_test("test2", test2)
    os.exit(0)
end

run_tests()
