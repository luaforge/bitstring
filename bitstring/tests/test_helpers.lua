require "os"
require "package"
require "bitstring"

--print = function(...) end

local run_test = function(name, f)
    print(string.rep("-", 32))
    print("started "..name)
    print(string.rep("-", 32))
    f()
    print(string.rep("-", 32))
    print("ended "..name)
    print(string.rep("-", 32))
end

local hexdump = function (x)
    if type(x) == "number" then
        print(string.format("%08x\n", x))
    else
        print(bitstring.hexdump(x))
    end
end


local assert_throw = function(f, message)
    result, e = pcall(f)
    if (result ~= true) and (string.find(e, message, 1, true) ~= nil) then 
	    if os.getenv("BITSTRING_IN_VISUAL_STUDIO") == nill then
            print("expected exception", e, " was thrown")
        else
            print("expected exception", " was thrown")
        end
        return true;
    else
        error((message or "expected exception").." not thrown")
    end
end


local assert_equal = function(result, expected)
    print("expected")
    hexdump(expected)
    print("result")
    hexdump(result)
    assert(result == expected)
 end

local assert_float_equal = function(result, expected, precision)
    print("expected")
    print(string.format("%f", expected))
    print("result")
    print(string.format("%f", result))
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
test_helpers.hexdump = hexdump


