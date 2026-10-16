local json = require "json"
local socket = require "socket"

local client

local M = {}

function M.connect(port_num)
    local address = "localhost"

    print("Connecting to port: " .. port_num)

    client = socket.tcp()

    -- If not connected within 3 seconds, throw an error
    client:settimeout(3)

    local valid, err = client:connect(address, port_num)
    if not valid then
        error("TCP connection failed: " .. tostring(err))
    end
end

function M.fetch_cities()
    local buffer, err = client:receive("*l")
    if not buffer then
        error("Failed to receive cities: " .. tostring(err))
    end

    local valid, decoded = pcall(json.decode, buffer)
    if not valid then
        error("Failed to parse JSON cities array: " .. tostring(decoded))
    end

    return decoded
end

function M.fetch_tour()
    local buffer, err = client:receive("*l")
    if not buffer then
        if err == "timeout" then
            return nil
        end

        error("Failed to receive tour: " .. err)
    end

    local valid, decoded = pcall(json.decode, buffer)
    if not valid then
        error("Failed to parse JSON tour array: " .. tostring(decoded))
    end

    return decoded
end

function M.close()
    if client then
        client:close()
        client = nil
    end
end

return M