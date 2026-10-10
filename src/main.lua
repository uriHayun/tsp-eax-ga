local ipc = require "ipc"
local map = require "map"

local FALLBACK_PORT_NUM = 56767

function love.load(arg)
    love.window.setMode(1280, 720, { resizable = true, vsync = true })

    ipc.connect(tonumber(arg[1]) or FALLBACK_PORT_NUM)

    local cities = ipc.fetch_cities()
    ipc.client:settimeout(0)

    MAP_IMG = map.fetch(map.build_url(cities))
end

function love.update()
    local tour = ipc.fetch_tour()
end

function love.draw()
end

function love.quit()
    ipc.close()
end