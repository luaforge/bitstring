require "os"
require "package"
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

local assert_float_equal = function(result, expected, precision)
    my_print("expected")
    my_print(string.format("%f", expected))
    my_print("result")
    my_print(string.format("%f", result))
    assert(math.abs(result - expected) < precision)
end


local assert_tables_equal = function(result, expected)
    for k, v in ipairs(result) do
        assert_equal(result[k], expected[k])
    end
end

local run_pack_unpack_test = function(pack_format, unpack_format, packed_values, expected_result)
    local result = bitstring.pack(pack_format, unpack(packed_values)) 
    assert_equal(result, expected_result)
    local unpacked_values = {bitstring.unpack(unpack_format, result)}
    assert_tables_equal(unpacked_values, packed_values)
end

module("test_helpers", package.seeall)
test_helpers.run_test = run_test
test_helpers.assert_equal = assert_equal
test_helpers.assert_float_equal = assert_float_equal
test_helpers.assert_tables_equal = assert_tables_equal
test_helpers.assert_throw = assert_throw
test_helpers.run_pack_unpack_test = run_pack_unpack_test
test_helpers.my_print = my_print
test_helpers.disable_print = disable_print
test_helpers.hexdump = hexdump


