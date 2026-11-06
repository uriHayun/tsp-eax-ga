local ipc = require("ipc")
local map = require("map")
local mercator = require("pseudo_mercator")

local FALLBACK_PORT_NUM = 56767 -- Inside of IANA private/dynamic range (49152–65535)

function love.load(arg)
    print("Lua started!")
    ipc.connect(tonumber(arg[1]) or FALLBACK_PORT_NUM)

    local cities_coords = ipc.fetch_cities()
    print("Cities valid!" .. #cities_coords)
    ipc.set_nonblocking()

    local view = map.get_view(cities_coords)
    MAP_IMG = assert(map.fetch(map.build_url(cities_coords, view)))
    print("Map valid!")

    SCALE, OFFSET_X, OFFSET_Y = map.get_map_placement(MAP_IMG)

    CITIES_ORDERED_PAIR = mercator.project_all(cities_coords)
    print("Done loading")
end

local got_tour = false
local ack_pending = false

function love.update()
    -- The previous tour was drawn last frame - so tell the backend to send the next one
    if ack_pending then
        ipc.ack()
        ack_pending = false
    end

    local tour = ipc.fetch_tour()
    if tour then
        CURR_TOUR = tour
        ack_pending = true

        if not got_tour then
            got_tour = true
            print("1st tour received!")
        end
    end
end

function love.draw()
    print("Start drawing")

    -- Reset background color to white to avoid tinting stuff when drawing
    love.graphics.setColor(1, 1, 1)

    -- Draw pre-rendered map image scaled to fill up the entire window (background)
    love.graphics.draw(MAP_IMG, OFFSET_X, OFFSET_Y, 0, SCALE, SCALE)

    -- Draw tour on map each generation
    if CURR_TOUR then
        local N = #CURR_TOUR
        local DARKEST = 0.25
        local BRIGHTEST = 1.0
        local red = BRIGHTEST
        local STEP = (BRIGHTEST - DARKEST) / math.max((N - 1), 1)

        for i = 1, N do
            -- C++ is 0-based, Lua is 1-based - so add to the tour indices
            local city = CITIES_ORDERED_PAIR[CURR_TOUR[i] + 1]
            local next_city = CITIES_ORDERED_PAIR[CURR_TOUR[i % N + 1] + 1]

            love.graphics.setColor(red, 0, 0)
            love.graphics.line(city.x, city.y, next_city.x, next_city.y)

            red = math.max(DARKEST, red - STEP)
        end
    end

    love.graphics.setColor(1, 1, 1)
end

function love.resize()
    print("Resizing")
    SCALE, OFFSET_X, OFFSET_Y = map.get_map_placement(MAP_IMG)
end

function love.quit()
    print("Quitting!")
    ipc.close()
end
