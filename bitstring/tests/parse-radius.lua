-- parse-radius.lua 
-- This example demonstrates parsing and creation of RADIUS protocol messages
-- using bitstring.pack and bitstring.unpack. It also uses bitstring.hexstream
-- and bitstring.fromhexstream utility functions that ease on debugging.
-- RADIUS protocol is defined in RFC 2865.
-- ---------------------------------------
require "bitstring"

-- copied from wireshark
packet = "01e40088abe03a1b2b38bbd7edcf08b26334f417010867696f7261300c06000005781e10303034302e393661302e343931301f10303034302e393662312e653036330606000000015012efbdb1bd5e4fe4147a95a78d001c51844f0d0202000b0167696f7261303d06000000130506000044f504060a383e83200f63642d617031313230622d3031"

radis_message = bitstring.fromhexstream(packet)

-- parse radius message
code, identifier, message_length, authenticator = 
    bitstring.unpack("8:int, 8:int, 16:int:big, 16:bin", radis_message)

attribute_list = {}
len = #radis_message - 20
start_pos = 21
end_pos = 22

while(len > 0) do
    number, attr_length = bitstring.unpack("8:int, 8:int", radis_message, start_pos, end_pos)
    value = bitstring.unpack((attr_length - 2) .. ":bin, rest:bin", radis_message, end_pos + 1, end_pos +  attr_length - 2)
    start_pos = start_pos + attr_length
    end_pos = start_pos + 1
    len = len - attr_length
    table.insert(attribute_list, {number = number, length = attr_length, value = value})
end

-- compose radius message
attributes = {}
for i, a in ipairs(attribute_list) do
    attribute = bitstring.pack("8:int, 8:int, all:bin", a.number, a.length, a.value)
    table.insert(attributes, attribute)
end
attributes = table.concat(attributes)

composed_message_length = 20 + #attributes
composed_radius_message = bitstring.pack("8:int, 8:int, 16:int:big, 16:bin, all:bin",
    code, identifier, composed_message_length, authenticator, attributes) 

-- print results
print("parsed radius message")
print("code: ", code)
print("identifier: ", identifier)
print("radius message length: ", message_length)
print("authenticator: ", bitstring.hexstream(authenticator))
print("attributes:")

for i, attribute in ipairs(attribute_list) do
    print("--------------------------------------")
    print("   number: ", attribute.number)
    print("   length: ", attribute.length)
    print("   value: ", bitstring.hexstream(attribute.value))
end
print("--------------------------------------")

print("composed radius message")
print(bitstring.hexstream(composed_radius_message))
assert(composed_radius_message == radis_message)
