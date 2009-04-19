require "os"
require "test_helpers"
require "bitstring"

local test1 = function()
    bitstring.hexdump("hello")
end


local run_tests = function()
    --disable_print()
    test_helpers.run_test("test1", test1)
    os.exit(0)
end

run_tests()
 

