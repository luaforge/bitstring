require "os"
require "bitstring"

local disable_print = function()
    io.output("/dev/null")
end

local my_print = function(...)
    for i, v in ipairs(arg) do
        io.write(tostring(v))
        io.write(" ")
    end
    io.write("\n")
end

local run_test = function(name, f)
    my_print(string.rep("-", 32))
    my_print("started "..name)
    my_print(string.rep("-", 32))
    f()
    my_print(string.rep("-", 32))
    my_print("ended "..name)
    my_print(string.rep("-", 32))
end

local hexdump = function (x)
    if type(x) == "number" then
        io.write(string.format("%08x\n", x))
    else
        hex = ''
        for b in string.gfind(x, ".") do
            io.write(string.format("%02x ", string.byte(b)))
        end
        io.write("\n")
    end
end


local assert_throw = function(f, message)
    result, e = pcall(f)
    if (result ~= true) and (string.find(e, message, 1, true) ~= nil) then 
        my_print("expected error", e, " was thrown")
        return true;
    else
        error((message or "expected error").." not thrown")
    end
end


local assert_equal = function(result, expected)
    my_print("expected")
    hexdump(expected)
    my_print("result")
    hexdump(result)
    assert(result == expected)
 end

local assert_tables_equal = function(result, expected)
    for k, v in ipairs(result) do
        assert_equal(result[k], expected[k])
    end
end

local run_pack_unpack_test = function(format, packed_values, expected_result)
    local result = bitstring.pack(format, unpack(packed_values)) 
    assert_equal(result, expected_result)
    local unpack_format = string.gsub(format, "all:bin", "rest:bin")
    local unpacked_values = {bitstring.unpack(unpack_format, result)}
    assert_tables_equal(unpacked_values, packed_values)
end

local test1 = function()
    assert_throw(function() bitstring.pack() end, "bad argument #1 to 'pack' (string expected, got no value)")
    assert_throw(function() bitstring.unpack("8:bin") end, "bad argument #2 to 'unpack' (string expected, got no value)")
end

local test2 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little, 5:bin"
    run_pack_unpack_test(format, packed_values, expected)
end

local test3 = function()
    local expected = "\1"
    local packed_values = {0x01}
    local format = "8:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test4 = function()
    local expected = "\1\1\2\1\2\3\4hello"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local format = "8:int, 16:int:big, 32:int:big, 5:bin"
    run_pack_unpack_test(format, packed_values, expected)
end

local test5 = function()
    local expected = "\255\255"
    --result = bitstring.pack("1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int, 1:int", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
    local packed_values = {}
    local format = string.rep("1:int,", 16)
    for i = 1, 16, 1 do packed_values[i] = 1 end
    run_pack_unpack_test(format, packed_values, expected)
end


local test6 = function()
    local expected = "\240\16\16\32\16\32\48\79"
    local packed_values = {0x0f, 0x1, 0x0102, 0x01020304, 0x0f}
    local format = "4:int, 8:int, 16:int:big, 32:int:big, 4:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test7 = function()
    local expected = "\255\255"
    local packed_values = {0x01, 0xff, 0x7f}
    local format = "1:int, 8:int, 7:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test8 = function()
    local expected = "\255\255"
    local packed_values = {0x07, 0x3f, 0x7f}
    local format = "3:int, 6:int, 7:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test9 = function()
    LUAL_BUFFERSIZE = 8192
    local expected = string.rep("\255", LUAL_BUFFERSIZE + 1)
    local packed_values = {0x07, string.rep("\255", LUAL_BUFFERSIZE), 0x1f}
    local format = "3:int, "..LUAL_BUFFERSIZE..":bin, 5:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test10 = function()
    local expected = "\160"
    local packed_values = {0x01, 0x0, 0x01, 0x0}
    local format = "1:int, 1:int, 1:int, 5:int"
    run_pack_unpack_test(format, packed_values, expected)
end

local test11 = function()
    local expected = "\170\170"
    local packed_values = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}
    local format = string.rep("1:int, ", 16)
    run_pack_unpack_test(format, packed_values, expected)
end

local test12 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little, all:bin"
    run_pack_unpack_test(format, packed_values, expected)
end

local test13 = function()
    local expected = "\1\2\1\4\3\2\1hell"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little, 4:bin"
    local result = bitstring.pack(format, unpack(packed_values))
    assert_equal(result, expected)
    local unpacked_values = {bitstring.unpack(format, result)}
    packed_values[4] = "hell"
    assert_tables_equal(packed_values, unpacked_values)
end

local test14 = function()
    assert_throw(
        function() 
            bitstring.pack("8:int:little, 16:int:little, 32:int:little, 6:bin", 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    assert_throw(
        function()
            bitstring.unpack("8:bin", "hello")
        end,
        "size error")
end

local test15 = function()
    assert_throw(
        function() 
            bitstring.pack("8:int:little, 16:int:little, 33:int:little, all:bin", 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    assert_throw(
        function() 
            bitstring.unpack("33:int:little", "\1\2\3\4") 
        end,
        "size error")

    assert_throw(
        function() 
            bitstring.unpack("33:int:little", "\1\2\3\4\5") 
        end,
        "size error")
end

local test16 = function()
    assert_throw(
        function() 
            bitstring.pack("8:int:little, 0:int:little, 32:int:little, all:bin", 
                0x1, 0x0102, 0x01020304, "hello")
        end,
        "size error")

    assert_throw(
        function() 
            bitstring.pack("0:int:little", "hello")
        end,
        "size error")
end

local test17 = function()
    local expected = "\1\2\1\4\3\2\1hello"
    local packed_values = {0x01, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little, all:bin "
    run_pack_unpack_test(format, packed_values, expected)
end

local test18 = function()
    local expected = "\1\2\1\4\3\2\1"
    local packed_values = {0x1, 0x0102, 0x01020304, "hello"}
    local format = "8:int:little, 16:int:little, 32:int:little,     "
    run_pack_unpack_test(format, packed_values, expected)
end

local test19 = function()
    assert_throw(
        function() 
            bitstring.pack(":int:little, 16:int:little", 0x1, 0x0102)
        end,
        "wrong format")

    assert_throw(
        function() 
            bitstring.pack("8:int:little, 16:int:littl", 0x1, 0x0102)
        end,
        "wrong format")

    assert_throw(
        function() 
            bitstring.pack("8:in:little, 16:int:little", 0x1, 0x0102)
        end,
        "wrong format")
end

local test20 = function()
    int8, int16, int32 = bitstring.unpack("8:int, 16:int:big, 32:int:little", "\1\1\2\1\2\3\4")
end

local test21 = function()
    local expected = "\255\255"
    local packed_values = {0xff, 0xff, 0xff}
    local format = "1:int, 8:int, 7:int"
    local result = bitstring.pack(format, unpack(packed_values))
    assert_equal(result, expected)
    local unpacked_values = {bitstring.unpack(format, expected)}
    local expected_values = {0x01, 0xff, 0x7f}
    assert_tables_equal(unpacked_values, expected_values)
end

local hex_to_bin = function(hexstream)
    -- assert valid input - hex digits, even
    -- remove spaces, etc.
    bint = {}
    for byte in string.gfind(hexstream, "(..)") do
        table.insert(bint, tonumber(byte, 16))
    end
    return string.char(unpack(bint))
end

peap_message_hex_stream = "010103f419c000000841160301004a02000046030149dc8f5707d07796bc76077dc2e169c22eff46a09809ae9c472ec420d03e99342010d83177ce0f88a7edc6fa1e691ee25c2ae75875f2e25929765355adc24abebc00350016030107e40b0007e00007dd0003c0308203bc30820325a0030201020203100002300d06092a864886f70d010104050030819c310b300906035504061302494c311e301c060355040813155374617465204f722050726f76696365204e616d653110300e060355040713074e6174616e6961310e300c060355040a1305436973636f31153013060355040b130c414353204469766973696f6e3111300f060355040313084341666f724143533121301f06092a864886f70d01090116124341666f7241435340636973636f2e636f6d301e170d3038313131303133333430345a170d3039313131303133333430345a30819f310b300906035504061302494c311f301d060355040813165374617465204f722050726f76696e6365204e616d653110300e060355040713074e6174616e6961310e300c060355040a1305436973636f31153013060355040b130c414353204469766973696f6e31123010060355040313094143535365727665723122302006092a864886f70d010901161341435353657276657240636973636f2e636f6d30819f300d06092a864886f70d010101050003818d0030818902818100d421b66c51bb0a667e213055718ee1725263dd28c6a4f25aff66428b435a292bfd33a3ced96604dbb69820dcfc8a3f56bea3747448676e1b6882e12e5fa915287452160adb4407527edc68d0628b8ba1ae5dcbaf632ca8c77146d2faef702e2be14cfb567c80c6019d3f45aea8ddcde84fa446292d10e9c875e6f386324e91390203010001a382010530820101300c0603551d130101ff04023000301f0603551d23041830168014ed84b9c28519344cc39a569e91c4023154be5c13301d0603551d0e04160414f9b2ed6245ec207c0c5ba61efe81b98d96c24140300b0603551d0f0404030205e0301d0603551d250416301406082b0601050507030106082b06010505070302301106096086480186f8420101040403020450304106096086480186f842010d043416325365727665722063657274696669636174652067656e657261746564207573696e67204f70656e53534c20666f7220414353302f06096086480186f842010404221620687474703a2f2f7777772e646f6d61696e2e646f6d2f63612d63726c2e70656d300d06092a864886f70d010104050003818100781544d883871860c4f4eeaca9e2f9a21e829d0df6cb58354e7f7ef8ef9d8d26cbbde559b2d23bfb5c43712dfee60575dfb1cecd0996108528c606563e73ed4242020524b1ec65e9c1b7ed6c"

EAP_REQUEST = 1
EAP_RESPONSE = 2
EAP_TYPE_PEAP = 0x19

local peap_example = function()
    local peap_message = hex_to_bin(peap_message_hex_stream)

    local code, identifier, length, eap_type, rest = 
        bitstring.unpack([[8:int, 
        8:int, 16:int, 8:int, rest:bin]], peap_message)
    assert(code == EAP_REQUEST)
    assert(eap_type == EAP_TYPE_PEAP)
    my_print("received peap message")

    local length_bit, more_bit, start_bit, reserved_bits, version, rest = 
        bitstring.unpack("1:int, 1:int, 1:int, 3:int, 2:int, rest:bin", rest)

    local peap_length = nil
    if length_bit == 1 then
        peap_length, rest = bitstring.unpack("32:int, rest:bin", rest)
    end

    if peap_length ~= nil then
        my_print("total peap message length: ", peap_length)
    end

    if more_bit == 1 then
        my_print("more fragments will follow, current fragment length: ", #rest)
    end

    if version == 0 then
        my_print("this is microsoft protocol version")
    elseif version == 1 then
        my_print("this is cisco protocol version")
    else
        my_print("error: unknown version")
    end
end


-- test more format arguments then actual arguments 
-- test less format arguments then actual arguments 
-- test sum of bits is not % 8
-- test unpack has more bits then input
-- test all or rest with bits % CHAR_BIT != 0
-- add memory leak test
-- add profiling test
-- test multiline format string
-- test very long tokens
-- example of loop



local run_tests = function()
    --disable_print()
    run_test("test21", test21)
    run_test("test20", test20)
    run_test("test19", test19)
    run_test("test18", test18)
    run_test("test17", test17)
    run_test("test16", test16)
    run_test("test15", test15)
    run_test("test14", test14)
    run_test("test13", test13)
    run_test("test12", test12)
    run_test("test11", test11)
    run_test("test10", test10)
    run_test("test9", test9)
    run_test("test8", test8)
    run_test("test7", test7)
    run_test("test6", test6)
    run_test("test5", test5)
    run_test("test4", test4)
    run_test("test3", test3)
    run_test("test2", test2)
    run_test("test1", test1)
    run_test("peap_example", peap_example)
    os.exit(0)
end

run_tests()
